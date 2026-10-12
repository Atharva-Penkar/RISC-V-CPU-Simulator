#include "rv/cpu.hpp"
#include "rv/decoder.hpp"

namespace rv {

namespace {

std::string signedValue(uint32_t value) { return std::to_string(static_cast<int32_t>(value)); }

const char* toString(ForwardSource source) {
    switch (source) {
        case ForwardSource::ExMem: return "EX/MEM";
        case ForwardSource::MemWb: return "MEM/WB";
        case ForwardSource::RegisterFile: break;
    }
    return "register file";
}

}  // namespace

Pipeline::Pipeline(InstructionMemory program) : imem_(program) {
    fetching_ = imem_.contains(0);
}

bool Pipeline::empty() const {
    return !fetching_ && !ifid_.valid && !idex_.valid && !exmem_.valid && !memwb_.valid;
}

PendingWrite Pipeline::pendingWrite(const ExMem& latch) const {
    return {latch.valid && !latch.fault && latch.cw.regWrite, latch.inst.rd};
}

PendingWrite Pipeline::pendingWrite(const MemWb& latch) const {
    return {latch.valid && latch.cw.regWrite, latch.inst.rd};
}

RunResult Pipeline::run(Tracer& tracer, std::size_t maxCycles) {
    RunStats& stats = result_.stats;
    while (!empty()) {
        if (stats.cycles == maxCycles) {
            result_.outcome = Outcome::CycleLimit;
            return result_;
        }
        stats.cycles++;
        CycleEvents events;
        const std::optional<Outcome> outcome = cycle(events);
        if (tracer.enabled()) {
            tracer.beginCycle(stats.cycles);
            trace(tracer, events);
        }
        if (outcome) {
            result_.outcome = *outcome;
            return result_;
        }
    }
    result_.outcome = Outcome::Finished;
    return result_;
}

/*every stage works on the pipeline registers as they were at the start of the cycle;
the new values are committed together at the end, like a clock edge*/
std::optional<Outcome> Pipeline::cycle(CycleEvents& events) {
    RunStats& stats = result_.stats;
    auto stop = [&](Outcome outcome) {
        ifid_ = {};     // younger instructions are discarded
        idex_ = {};
        exmem_ = {};
        memwb_ = {};
        return outcome;
    };

    /*WB: writes the register file in the first half of the cycle, so ID reads the new value*/
    if (memwb_.valid) {
        if (memwb_.cw.regWrite)
            regs_.write(memwb_.inst.rd, memwb_.result);
        stats.retired++;
    }

    /*MEM: faults and halts take effect here, after every older instruction has completed*/
    MemWb nextMemWb;
    if (exmem_.valid) {
        if (exmem_.fault) {
            result_.fault = exmem_.fault;
            return stop(Outcome::Faulted);
        }
        if (exmem_.cw.halt) {
            stats.retired++;
            result_.haltPc = exmem_.pc;
            return stop(Outcome::Halted);
        }
        const MemoryResult mem = accessMemory(dmem_, exmem_.inst, exmem_.cw, exmem_.pc, exmem_.aluOut, exmem_.storeData);
        if (mem.fault) {
            result_.fault = mem.fault;
            return stop(Outcome::Faulted);
        }
        nextMemWb = {true, exmem_.pc, exmem_.inst, exmem_.cw, exmem_.cw.memToReg ? mem.loaded : exmem_.aluOut};
    }

    /*EX: operands are forwarded from the two older instructions that have not written back yet*/
    ExMem nextExMem;
    std::optional<uint32_t> redirect;
    if (idex_.valid) {
        if (idex_.fault)
            nextExMem = {true, idex_.pc, {}, {}, 0, 0, idex_.fault};
        else {
            const PendingWrite fromExMem = pendingWrite(exmem_);
            const PendingWrite fromMemWb = pendingWrite(memwb_);
            auto operand = [&](Reg source, uint32_t registerValue, ForwardSource& used) {
                used = HazardUnit::forward(source, fromExMem, fromMemWb);
                switch (used) {
                    case ForwardSource::ExMem: return exmem_.aluOut;
                    case ForwardSource::MemWb: return memwb_.result;
                    case ForwardSource::RegisterFile: break;
                }
                return registerValue;
            };
            const uint32_t rs1 = operand(idex_.inst.rs1, idex_.rs1, events.rs1);
            const uint32_t rs2 = operand(idex_.inst.rs2, idex_.rs2, events.rs2);
            const ExecuteResult ex = execute(idex_.inst, idex_.cw, idex_.pc, rs1, rs2);
            nextExMem = {true, idex_.pc, idex_.inst, idex_.cw, ex.value, rs2, std::nullopt};
            redirect = ex.target;
        }
    }

    /*ID: decode and read registers; a load in EX whose result is needed here costs one stall cycle*/
    IdEx nextIdEx;
    bool stall = false;
    if (ifid_.valid) {
        if (ifid_.fault)
            nextIdEx = {true, ifid_.pc, {}, {}, 0, 0, ifid_.fault};
        else if (const std::optional<DecodedInst>& inst = imem_.decoded(ifid_.pc)) {
            const Control cw = controlFor(inst->kind());
            const PendingWrite loadInEx{idex_.valid && !idex_.fault && idex_.cw.memRead, idex_.inst.rd};
            if (HazardUnit::loadUseHazard(*inst, cw, loadInEx))
                stall = true;   // ID/EX receives a bubble
            else
                nextIdEx = {true, ifid_.pc, *inst, cw, regs_.read(inst->rs1), regs_.read(inst->rs2), std::nullopt};
        }
        else
            nextIdEx = {true, ifid_.pc, {}, {}, 0, 0, Fault{FaultKind::IllegalInstruction, ifid_.pc, ifid_.ir}};
    }

    /*IF: a stall holds the PC and IF/ID*/
    IfId nextIfId = ifid_;
    if (!stall) {
        nextIfId = {};
        if (fetching_) {
            if (imem_.contains(pc_)) {
                nextIfId = {true, pc_, imem_.fetch(pc_), std::nullopt};
                pc_ += 4;
            }
            else {
                fetching_ = false;
                if (pc_ != imem_.endAddress()) {
                    const FaultKind kind = pc_ % 4 != 0 ? FaultKind::FetchMisaligned : FaultKind::FetchOutOfRange;
                    nextIfId = {true, pc_, 0, Fault{kind, pc_, pc_}};
                }
            }
        }
    }

    /*a taken branch or jump in EX: what IF fetched and ID decoded in this cycle is on the wrong path*/
    if (redirect) {
        events.flushed = static_cast<std::size_t>(nextIfId.valid) + static_cast<std::size_t>(nextIdEx.valid);
        stats.flushes += events.flushed;
        stats.redirects++;
        nextIfId = {};
        nextIdEx = {};
        pc_ = *redirect;
        fetching_ = true;
    }
    if (stall) {
        stats.stalls++;
        events.stalled = true;
    }

    ifid_ = nextIfId;
    idex_ = nextIdEx;
    exmem_ = nextExMem;
    memwb_ = nextMemWb;
    return std::nullopt;
}

void Pipeline::trace(Tracer& tracer, const CycleEvents& events) const {
    auto faultText = [](const std::optional<Fault>& fault) { return "FAULT: " + describe(*fault); };
    tracer.stage("PC", std::to_string(pc_) + (fetching_ ? "" : " (not fetching)"));
    tracer.stage("IF/ID", !ifid_.valid ? "-"
                          : ifid_.fault ? faultText(ifid_.fault)
                                        : "pc=" + std::to_string(ifid_.pc) + " ir=" + toHex(ifid_.ir));
    tracer.stage("ID/EX", !idex_.valid ? "-"
                          : idex_.fault ? faultText(idex_.fault)
                                        : disassemble(idex_.inst) + "  pc=" + std::to_string(idex_.pc)
                                              + " rs1=" + signedValue(idex_.rs1) + " rs2=" + signedValue(idex_.rs2)
                                              + " " + toString(idex_.cw));
    tracer.stage("EX/MEM", !exmem_.valid ? "-"
                           : exmem_.fault ? faultText(exmem_.fault)
                                          : disassemble(exmem_.inst) + "  aluout=" + signedValue(exmem_.aluOut)
                                                + " rs2=" + signedValue(exmem_.storeData));
    tracer.stage("MEM/WB", !memwb_.valid ? "-" : disassemble(memwb_.inst) + "  result=" + signedValue(memwb_.result));

    std::string hazards;
    auto add = [&](const std::string& text) { hazards += (hazards.empty() ? "" : "; ") + text; };
    if (events.stalled)
        add("load-use stall");
    if (events.flushed != 0)
        add("flushed " + std::to_string(events.flushed));
    if (events.rs1 != ForwardSource::RegisterFile)
        add(std::string("rs1 <- ") + toString(events.rs1));
    if (events.rs2 != ForwardSource::RegisterFile)
        add(std::string("rs2 <- ") + toString(events.rs2));
    if (!hazards.empty())
        tracer.stage("hazards", hazards);
}

}  // namespace rv
