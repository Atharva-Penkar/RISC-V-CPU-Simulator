#include "rv/cpu.hpp"
#include "rv/decoder.hpp"

namespace rv {

RunResult Iss::run(Tracer& tracer, std::size_t maxInstructions) {
    RunResult result;
    uint32_t pc = 0;
    while (imem_.contains(pc)) {
        if (result.cycles == maxInstructions)
            return result;
        result.cycles++;
        tracer.beginCycle(result.cycles);
        const uint32_t word = imem_.fetch(pc);
        const std::optional<DecodedInst> inst = decode(word);
        if (!inst) {
            tracer.event("unknown instruction " + toHex(word) + " at " + toHex(pc) + " is skipped");
            pc += 4;
            continue;
        }
        tracer.stage("EXEC", toHex(pc) + "  " + disassemble(*inst));
        const Control control = controlFor(inst->kind());
        const ExecuteResult ex = execute(*inst, control, pc, regs_.read(inst->rs1), regs_.read(inst->rs2));

        uint32_t loaded = 0;
        if (control.memRead) {
            const std::optional<uint32_t> value = dmem_.load(inst->spec->funct3, ex.value);
            if (!value)
                tracer.event("invalid memory access at address " + toHex(ex.value) + " is ignored");
            loaded = value.value_or(0);
        }
        if (control.memWrite && !dmem_.store(inst->spec->funct3, ex.value, regs_.read(inst->rs2)))
            tracer.event("invalid memory access at address " + toHex(ex.value) + " is ignored");
        if (control.regWrite)
            regs_.write(inst->rd, control.memToReg ? loaded : ex.value);
        result.retired++;

        pc = ex.target.value_or(pc + 4);
        if (ex.target && !imem_.contains(pc) && pc != imem_.endAddress())
            tracer.event("jump/branch target " + toHex(pc) + " is outside the program");
    }
    result.completed = true;
    return result;
}

}  // namespace rv
