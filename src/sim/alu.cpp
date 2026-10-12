#include "rv/cpu.hpp"

namespace rv {

Control controlFor(Kind kind) {
    Control c;
    switch (kind) {
        case Kind::Alu:
            c.readsRs1 = c.readsRs2 = c.regWrite = true;
            break;
        case Kind::AluImm:
        case Kind::Shift:
            c.readsRs1 = c.aluSrc = c.regWrite = true;
            break;
        case Kind::Load:
            c.readsRs1 = c.aluSrc = c.regWrite = c.memRead = c.memToReg = true;
            break;
        case Kind::Store:
            c.readsRs1 = c.readsRs2 = c.aluSrc = c.memWrite = true;
            break;
        case Kind::Branch:
            c.readsRs1 = c.readsRs2 = c.branch = true;
            break;
        case Kind::Lui:
        case Kind::Auipc:
            c.aluSrc = c.regWrite = true;
            break;
        case Kind::Jal:
            c.aluSrc = c.regWrite = c.jump = true;
            break;
        case Kind::Jalr:
            c.readsRs1 = c.aluSrc = c.regWrite = c.jump = true;
            break;
    }
    return c;
}

std::string toString(const Control& c) {
    std::string flags;
    auto add = [&](bool set, const char* name) {
        if (!set)
            return;
        if (!flags.empty())
            flags += ' ';
        flags += name;
    };
    add(c.readsRs1, "RegRead1");
    add(c.readsRs2, "RegRead2");
    add(c.aluSrc, "ALUSrc");
    add(c.regWrite, "RegWrite");
    add(c.memRead, "MemRead");
    add(c.memWrite, "MemWrite");
    add(c.memToReg, "Mem2Reg");
    add(c.branch, "Branch");
    add(c.jump, "Jump");
    return "[" + flags + "]";
}

/*arithmetic is unsigned so overflow wraps; signed views only where the ISA needs them*/
uint32_t aluExecute(AluOp op, uint32_t a, uint32_t b) {
    const auto sa = static_cast<int32_t>(a);
    const auto sb = static_cast<int32_t>(b);
    const uint32_t shamt = b & 31u;
    switch (op) {
        case AluOp::Add: return a + b;
        case AluOp::Sub: return a - b;
        case AluOp::And: return a & b;
        case AluOp::Or: return a | b;
        case AluOp::Xor: return a ^ b;
        case AluOp::Sll: return a << shamt;
        case AluOp::Srl: return a >> shamt;
        case AluOp::Sra: return static_cast<uint32_t>(sa >> shamt);
        case AluOp::Slt: return sa < sb;
        case AluOp::Sltu: return a < b;
        case AluOp::Eq: return a == b;
        case AluOp::Ne: return a != b;
        case AluOp::Lt: return sa < sb;
        case AluOp::Ge: return sa >= sb;
        case AluOp::Ltu: return a < b;
        case AluOp::Geu: return a >= b;
        case AluOp::PassB: return b;
    }
    return 0;
}

ExecuteResult execute(const DecodedInst& inst, const Control& control, uint32_t pc, uint32_t rs1, uint32_t rs2) {
    const bool pcRelative = inst.kind() == Kind::Auipc || inst.kind() == Kind::Jal;
    const uint32_t a = pcRelative ? pc : rs1;
    const uint32_t b = control.aluSrc ? static_cast<uint32_t>(inst.imm) : rs2;
    const uint32_t aluOut = aluExecute(inst.spec->alu, a, b);
    if (control.branch)
        return {aluOut, aluOut != 0 ? std::optional(pc + static_cast<uint32_t>(inst.imm)) : std::nullopt};
    if (control.jump)
        return {pc + 4, aluOut & ~1u};     // link address; target pc+imm (JAL) or (rs1+imm)&~1 (JALR)
    return {aluOut, std::nullopt};
}

}  // namespace rv
