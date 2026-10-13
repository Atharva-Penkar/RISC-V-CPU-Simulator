#include "rv/cpu.hpp"
#include "rv/decoder.hpp"
#include "rv/timeline.hpp"

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

uint32_t Pipeline::newInstance(uint32_t pc) {
    const uint32_t id = nextInstance_++;
    if (timeline_) {
        InstanceRecord record;
        record.id = id;
        record.pc = pc;
        record.fetchCycle = result_.stats.cycles;
        if (imem_.contains(pc)) {
            record.instruction = pc / 4;
            record.occurrence = ++timeline_->occurrences_[pc / 4];
        }
        timeline_->instances_.push_back(record);
    }
    return id;
}

namespace {

template <class Latch>
StageSlot slotOf(const Latch& latch) {
    StageSlot slot;
    if (latch.valid) {
        slot.kind = StageSlot::Kind::Instruction;
        slot.instance = latch.instance;
    }
    else if (latch.bubble) {
        slot.kind = StageSlot::Kind::Bubble;
        slot.bubble = *latch.bubble;
    }
    return slot;
}

constexpr std::size_t index(Stage stage) { return static_cast<std::size_t>(stage); }

}  // namespace

/*every stage works on the pipeline registers as they were at the start of the cycle;
the new values are committed together at the end, like a clock edge*/
std::optional<Outcome> Pipeline::cycle(CycleEvents& events) {
    RunStats& stats = result_.stats;

    /*the timeline record of this cycle; the ID, EX, MEM and WB stages work on the latches
    as they were at the start of the cycle, IF on what it fetches*/
    CycleRecord* record = nullptr;
    if (timeline_) {
        record = &timeline_->cycles_.emplace_back();
        record->cycle = stats.cycles;
        record->pc = pc_;
        record->stages[index(Stage::ID)] = slotOf(ifid_);
        record->stages[index(Stage::EX)] = slotOf(idex_);
        record->stages[index(Stage::MEM)] = slotOf(exmem_);
        record->stages[index(Stage::WB)] = slotOf(memwb_);
    }
    auto settle = [&](uint32_t instance, Fate fate) {
        if (timeline_) {
            InstanceRecord& entry = timeline_->instances_.at(instance);
            entry.fate = fate;
            entry.endCycle = stats.cycles;
        }
    };
    auto stop = [&](Outcome outcome, uint32_t instance, Fate fate) {
        settle(instance, fate);
        if (timeline_) {
            timeline_->stopInstance_ = instance;
            if (ifid_.valid)    // younger instructions are discarded
                settle(ifid_.instance, Fate::Discarded);
            if (idex_.valid)
                settle(idex_.instance, Fate::Discarded);
            if (pendingFetch_)
                settle(*pendingFetch_, Fate::Discarded);
        }
        ifid_ = {};
        idex_ = {};
        exmem_ = {};
        memwb_ = {};
        pendingFetch_.reset();
        return outcome;
    };

    /*WB: writes the register file in the first half of the cycle, so ID reads the new value*/
    if (memwb_.valid) {
        if (memwb_.cw.regWrite) {
            if (record && memwb_.inst.rd.index() != 0) {
                const WriteSource source = memwb_.cw.memToReg ? WriteSource::Memory
                                         : memwb_.cw.jump     ? WriteSource::Link
                                                              : WriteSource::Alu;
                record->registerWrite = RegisterWriteRecord{memwb_.instance, memwb_.inst.rd, memwb_.result,
                                                            regs_.read(memwb_.inst.rd), source};
            }
            regs_.write(memwb_.inst.rd, memwb_.result);
        }
        stats.retired++;
        settle(memwb_.instance, Fate::Retired);
    }

    /*MEM: faults and halts take effect here, after every older instruction has completed*/
    MemWb nextMemWb;
    if (exmem_.valid) {
        if (exmem_.fault) {
            result_.fault = exmem_.fault;
            return stop(Outcome::Faulted, exmem_.instance, Fate::Faulted);
        }
        if (exmem_.cw.halt) {
            stats.retired++;
            result_.haltPc = exmem_.pc;
            return stop(Outcome::Halted, exmem_.instance, Fate::Halted);
        }
        const uint32_t width = DataMemory::accessWidth(exmem_.inst.spec->funct3);
        const uint32_t overwritten = exmem_.cw.memWrite ? dmem_.read(exmem_.aluOut, width).value_or(0) : 0;
        const MemoryResult mem = accessMemory(dmem_, exmem_.inst, exmem_.cw, exmem_.pc, exmem_.aluOut, exmem_.storeData);
        if (mem.fault) {
            result_.fault = mem.fault;
            return stop(Outcome::Faulted, exmem_.instance, Fate::Faulted);
        }
        if (record && (exmem_.cw.memRead || exmem_.cw.memWrite)) {
            const uint32_t mask = width == 4 ? 0xFFFFFFFFu : (1u << (8 * width)) - 1;
            const uint32_t value = exmem_.cw.memWrite ? exmem_.storeData & mask : mem.loaded;
            record->memory = MemoryRecord{exmem_.instance, exmem_.cw.memWrite, exmem_.aluOut, width, value, overwritten};
        }
        nextMemWb = {true, exmem_.pc, exmem_.inst, exmem_.cw, exmem_.cw.memToReg ? mem.loaded : exmem_.aluOut,
                     exmem_.instance, std::nullopt};
    }
    else
        nextMemWb.bubble = exmem_.bubble;

    /*EX: operands are forwarded from the two older instructions that have not written back yet*/
    ExMem nextExMem;
    std::optional<uint32_t> redirect;
    if (idex_.valid) {
        if (idex_.fault)
            nextExMem = {true, idex_.pc, {}, {}, 0, 0, idex_.fault, idex_.instance, std::nullopt};
        else {
            const PendingWrite fromExMem = pendingWrite(exmem_);
            const PendingWrite fromMemWb = pendingWrite(memwb_);
            auto operand = [&](Operand which, Reg source, uint32_t registerValue, ForwardSource& used) {
                used = HazardUnit::forward(source, fromExMem, fromMemWb);
                uint32_t value = registerValue;
                if (used == ForwardSource::ExMem)
                    value = exmem_.aluOut;
                else if (used == ForwardSource::MemWb)
                    value = memwb_.result;
                if (record && used != ForwardSource::RegisterFile) {
                    const uint32_t producer = used == ForwardSource::ExMem ? exmem_.instance : memwb_.instance;
                    record->forwards.push_back({which, used, source, value, producer, idex_.instance});
                }
                return value;
            };
            const uint32_t rs1 = operand(Operand::Rs1, idex_.inst.rs1, idex_.rs1, events.rs1);
            const uint32_t rs2 = operand(Operand::Rs2, idex_.inst.rs2, idex_.rs2, events.rs2);
            const ExecuteResult ex = execute(idex_.inst, idex_.cw, idex_.pc, rs1, rs2);
            nextExMem = {true, idex_.pc, idex_.inst, idex_.cw, ex.value, rs2, std::nullopt, idex_.instance, std::nullopt};
            redirect = ex.target;
            if (record) {
                ExecuteRecord& exec = record->execute.emplace();
                exec.instance = idex_.instance;
                exec.op = idex_.inst.spec->alu;
                exec.a = ex.aluA;
                exec.b = ex.aluB;
                exec.result = ex.aluOut;
                exec.aFromPc = idex_.inst.kind() == Kind::Auipc || idex_.inst.kind() == Kind::Jal;
                exec.bFromImmediate = idex_.cw.aluSrc;
                if (idex_.cw.branch)
                    exec.branchTaken = ex.target.has_value();
                exec.target = ex.target;
            }
        }
    }
    else
        nextExMem.bubble = idex_.bubble;

    /*ID: decode and read registers; a load in EX whose result is needed here costs one stall cycle*/
    IdEx nextIdEx;
    bool stall = false;
    if (ifid_.valid) {
        if (ifid_.fault)
            nextIdEx = {true, ifid_.pc, {}, {}, 0, 0, ifid_.fault, ifid_.instance, std::nullopt};
        else if (const std::optional<DecodedInst>& inst = imem_.decoded(ifid_.pc)) {
            const Control cw = controlFor(inst->kind());
            const PendingWrite loadInEx{idex_.valid && !idex_.fault && idex_.cw.memRead, idex_.inst.rd};
            if (HazardUnit::loadUseHazard(*inst, cw, loadInEx)) {
                stall = true;
                nextIdEx.bubble = BubbleReason::Stall;
                if (record) {
                    record->stall = StallRecord{ifid_.instance, idex_.instance, idex_.inst.rd};
                    record->stages[index(Stage::ID)].stalled = true;
                }
            }
            else
                nextIdEx = {true, ifid_.pc, *inst, cw, regs_.read(inst->rs1), regs_.read(inst->rs2), std::nullopt,
                            ifid_.instance, std::nullopt};
        }
        else
            nextIdEx = {true, ifid_.pc, {}, {}, 0, 0, Fault{FaultKind::IllegalInstruction, ifid_.pc, ifid_.ir},
                        ifid_.instance, std::nullopt};
    }
    else
        nextIdEx.bubble = ifid_.bubble;

    /*IF: a stall holds the PC and IF/ID; the instruction at the PC is fetched again next cycle*/
    IfId nextIfId = ifid_;
    NextPcSource nextPcSource = NextPcSource::Stopped;
    if (stall) {
        nextPcSource = NextPcSource::Hold;
        if (fetching_ && imem_.contains(pc_)) {
            if (!pendingFetch_)
                pendingFetch_ = newInstance(pc_);
            if (record)
                record->stages[index(Stage::IF)] = {StageSlot::Kind::Instruction, *pendingFetch_, BubbleReason::Stall, true};
        }
    }
    else {
        nextIfId = {};
        if (fetching_) {
            if (imem_.contains(pc_)) {
                const uint32_t instance = pendingFetch_ ? *pendingFetch_ : newInstance(pc_);
                pendingFetch_.reset();
                nextIfId = {true, pc_, imem_.fetch(pc_), std::nullopt, instance, std::nullopt};
                pc_ += 4;
                nextPcSource = NextPcSource::Sequential;
            }
            else {
                fetching_ = false;
                if (pc_ != imem_.endAddress()) {
                    const FaultKind kind = pc_ % 4 != 0 ? FaultKind::FetchMisaligned : FaultKind::FetchOutOfRange;
                    nextIfId = {true, pc_, 0, Fault{kind, pc_, pc_}, newInstance(pc_), std::nullopt};
                }
            }
            if (record && nextIfId.valid)
                record->stages[index(Stage::IF)] = slotOf(nextIfId);
        }
    }

    /*a taken branch or jump in EX: what IF fetched and ID decoded in this cycle is on the wrong path*/
    if (redirect) {
        events.flushed = static_cast<std::size_t>(nextIfId.valid) + static_cast<std::size_t>(nextIdEx.valid);
        stats.flushes += events.flushed;
        stats.redirects++;
        if (record) {
            FlushRecord flush{idex_.instance, {}, *redirect};
            for (const auto& [valid, instance] : {std::pair{nextIdEx.valid, nextIdEx.instance},
                                                  std::pair{nextIfId.valid, nextIfId.instance}}) {
                if (valid) {
                    flush.squashed.push_back(instance);
                    settle(instance, Fate::Flushed);
                }
            }
            record->flush = std::move(flush);
        }
        nextIfId = {};
        nextIdEx = {};
        nextIfId.bubble = BubbleReason::Flush;
        nextIdEx.bubble = BubbleReason::Flush;
        pc_ = *redirect;
        fetching_ = true;
        nextPcSource = idex_.cw.branch ? NextPcSource::BranchTarget : NextPcSource::JumpTarget;
    }
    if (stall) {
        stats.stalls++;
        events.stalled = true;
    }
    if (record) {
        record->nextPcSource = nextPcSource;
        record->nextPc = pc_;
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
