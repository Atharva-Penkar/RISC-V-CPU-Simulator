#include "rv/cpu.hpp"
#include "rv/decoder.hpp"

namespace rv {

namespace {

/*fetch fault for an address outside the program, other than the end address*/
std::optional<Fault> fetchFault(const InstructionMemory& imem, uint32_t pc) {
    if (imem.contains(pc) || pc == imem.endAddress())
        return std::nullopt;
    return Fault{pc % 4 != 0 ? FaultKind::FetchMisaligned : FaultKind::FetchOutOfRange, pc, pc};
}

}  // namespace

RunResult Iss::run(Tracer& tracer, std::size_t maxCycles) {
    RunResult result;
    RunStats& stats = result.stats;
    uint32_t pc = 0;
    while (true) {
        if (auto fault = fetchFault(imem_, pc)) {
            result.outcome = Outcome::Faulted;
            result.fault = fault;
            return result;
        }
        if (!imem_.contains(pc)) {
            result.outcome = Outcome::Finished;
            return result;
        }
        if (stats.cycles == maxCycles) {
            result.outcome = Outcome::CycleLimit;
            return result;
        }
        stats.cycles++;
        tracer.beginCycle(stats.cycles);
        const uint32_t word = imem_.fetch(pc);
        const std::optional<DecodedInst> inst = decode(word);
        if (!inst) {
            result.outcome = Outcome::Faulted;
            result.fault = Fault{FaultKind::IllegalInstruction, pc, word};
            return result;
        }
        tracer.stage("EXEC", toHex(pc) + "  " + disassemble(*inst));
        const Control control = controlFor(inst->kind());
        if (control.halt) {
            stats.retired++;
            result.outcome = Outcome::Halted;
            result.haltPc = pc;
            return result;
        }
        const uint32_t rs2 = regs_.read(inst->rs2);
        const ExecuteResult ex = execute(*inst, control, pc, regs_.read(inst->rs1), rs2);
        const MemoryResult mem = accessMemory(dmem_, *inst, control, pc, ex.value, rs2);
        if (mem.fault) {
            result.outcome = Outcome::Faulted;
            result.fault = mem.fault;
            return result;
        }
        if (control.regWrite)
            regs_.write(inst->rd, control.memToReg ? mem.loaded : ex.value);
        stats.retired++;
        if (ex.target)
            stats.redirects++;
        pc = ex.target.value_or(pc + 4);
    }
}

}  // namespace rv
