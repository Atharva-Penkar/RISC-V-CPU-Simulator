#include "rv/cpu.hpp"
#include "rv/decoder.hpp"

namespace rv {

namespace {

std::string signedValue(uint32_t value) { return std::to_string(static_cast<int32_t>(value)); }

}  // namespace

Pipeline::Pipeline(InstructionMemory program) : imem_(program) {
    pc_.valid = imem_.contains(0);
}

bool Pipeline::busy() const {
    return pc_.valid || ifid_.valid || idex_.valid || exmem_.valid || memwb_.valid;
}

RunResult Pipeline::run(Tracer& tracer, std::size_t maxCycles) {
    RunResult result;
    while (busy()) {
        if (result.cycles == maxCycles) {
            result.retired = retired_;
            return result;
        }
        result.cycles++;
        tracer.beginCycle(result.cycles);
        /*stages run in reverse so each one consumes its input latch before the previous stage refills it*/
        writeBackStage();
        memoryStage(tracer);
        executeStage(tracer);
        decodeStage(tracer);
        fetch();
        traceState(tracer);
    }
    result.retired = retired_;
    result.completed = true;
    return result;
}

void Pipeline::fetch() {
    if (ifid_.valid || !pc_.valid)     // a stalled instruction still occupies IF/ID
        return;
    ifid_ = {true, false, pc_.value, imem_.fetch(pc_.value)};
    pc_.value += 4;
    pc_.valid = imem_.contains(pc_.value);
}

void Pipeline::decodeStage(Tracer& tracer) {
    if (!ifid_.valid)
        return;
    const std::optional<DecodedInst> inst = decode(ifid_.ir);
    if (!inst) {
        tracer.event("unknown instruction " + toHex(ifid_.ir) + " at " + toHex(ifid_.pc) + " is skipped");
        ifid_.valid = false;
        return;
    }
    const Control cw = controlFor(inst->kind());
    if (hazards_.mustStall(*inst, cw)) {
        ifid_.stall = true;     // ID/EX stays empty: a bubble enters the pipeline
        return;
    }
    idex_ = {true, *inst, cw, ifid_.pc, regs_.read(inst->rs1), regs_.read(inst->rs2), nextTag_++};
    hazards_.issue(*inst, cw, idex_.tag);
    ifid_ = IfId{};
}

void Pipeline::executeStage(Tracer& tracer) {
    if (!idex_.valid)
        return;
    const ExecuteResult ex = execute(idex_.inst, idex_.cw, idex_.pc, idex_.rs1, idex_.rs2);
    exmem_ = {true, idex_.inst, idex_.cw, ex.value, idex_.rs2, idex_.tag};
    idex_.valid = false;
    if (ex.target) {
        pc_.value = *ex.target;
        pc_.valid = imem_.contains(*ex.target);
        if (!pc_.valid && *ex.target != imem_.endAddress())
            tracer.event("jump/branch target " + toHex(*ex.target) + " is outside the program");
        /*the instruction fetched behind the branch is on the wrong path; it has not been decoded yet*/
        ifid_ = IfId{};
    }
}

void Pipeline::memoryStage(Tracer& tracer) {
    if (!exmem_.valid)
        return;
    const Control& cw = exmem_.cw;
    const uint32_t funct3 = exmem_.inst.spec->funct3;
    uint32_t loadData = 0;
    bool ok = true;
    if (cw.memRead) {
        const std::optional<uint32_t> value = dmem_.load(funct3, exmem_.aluOut);
        ok = value.has_value();
        loadData = value.value_or(0);
    }
    if (cw.memWrite)
        ok = dmem_.store(funct3, exmem_.aluOut, exmem_.storeData);
    if (!ok)
        tracer.event("invalid memory access at address " + toHex(exmem_.aluOut) + " is ignored");
    memwb_ = {true, exmem_.inst, cw, exmem_.aluOut, loadData, exmem_.tag};
    exmem_.valid = false;
}

void Pipeline::writeBackStage() {
    if (!memwb_.valid)
        return;
    if (memwb_.cw.regWrite) {
        regs_.write(memwb_.inst.rd, memwb_.cw.memToReg ? memwb_.loadData : memwb_.aluOut);
        hazards_.writeBack(memwb_.inst.rd, memwb_.tag);
    }
    memwb_.valid = false;
    retired_++;
}

void Pipeline::traceState(Tracer& tracer) const {
    tracer.stage("PC", std::to_string(pc_.value) + (pc_.valid ? "" : " (done)"));
    tracer.stage("IF/ID", !ifid_.valid ? "-"
                          : "pc=" + std::to_string(ifid_.pc) + " ir=" + toHex(ifid_.ir) + (ifid_.stall ? "  STALL" : ""));
    tracer.stage("ID/EX", !idex_.valid ? "-"
                          : disassemble(idex_.inst) + "  pc=" + std::to_string(idex_.pc) + " rs1=" + signedValue(idex_.rs1)
                                + " rs2=" + signedValue(idex_.rs2) + " " + toString(idex_.cw));
    tracer.stage("EX/MEM", !exmem_.valid ? "-"
                           : disassemble(exmem_.inst) + "  aluout=" + signedValue(exmem_.aluOut)
                                 + " rs2=" + signedValue(exmem_.storeData));
    tracer.stage("MEM/WB", !memwb_.valid ? "-"
                           : disassemble(memwb_.inst) + "  aluout=" + signedValue(memwb_.aluOut)
                                 + " ldout=" + signedValue(memwb_.loadData));
}

}  // namespace rv
