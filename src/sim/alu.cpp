#include "rv/cpu.hpp"
#include "rv/decoder.hpp"

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
        case Kind::System:
            c.halt = true;
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
    add(c.halt, "Halt");
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

MemoryResult accessMemory(DataMemory& memory, const DecodedInst& inst, const Control& control, uint32_t pc,
                          uint32_t address, uint32_t storeData) {
    if (!control.memRead && !control.memWrite)
        return {};
    const uint32_t funct3 = inst.spec->funct3;
    switch (memory.check(address, DataMemory::accessWidth(funct3))) {
        case MemoryError::OutOfRange:
            return {0, Fault{control.memRead ? FaultKind::LoadOutOfRange : FaultKind::StoreOutOfRange, pc, address}};
        case MemoryError::Misaligned:
            return {0, Fault{control.memRead ? FaultKind::LoadMisaligned : FaultKind::StoreMisaligned, pc, address}};
        case MemoryError::None:
            break;
    }
    if (control.memWrite) {
        memory.store(funct3, address, storeData);
        return {};
    }
    return {memory.load(funct3, address).value_or(0), std::nullopt};
}

std::string describe(const Fault& fault) {
    const std::string at = " at pc " + toHex(fault.pc) + ": ";
    const std::string address = toHex(fault.address);
    switch (fault.kind) {
        case FaultKind::IllegalInstruction:
            return "illegal instruction" + at + address + " is not an RV32I instruction";
        case FaultKind::FetchOutOfRange:
            return "instruction fetch fault: jump target " + address + " is outside the program";
        case FaultKind::FetchMisaligned:
            return "instruction fetch fault: jump target " + address + " is not word aligned";
        case FaultKind::LoadOutOfRange:
            return "load access fault" + at + "address " + address + " is outside data memory";
        case FaultKind::LoadMisaligned:
            return "load access fault" + at + "address " + address + " is misaligned";
        case FaultKind::StoreOutOfRange:
            return "store access fault" + at + "address " + address + " is outside data memory";
        case FaultKind::StoreMisaligned:
            return "store access fault" + at + "address " + address + " is misaligned";
    }
    return "fault" + at;
}

std::string toString(Outcome outcome) {
    switch (outcome) {
        case Outcome::Finished: return "finished";
        case Outcome::Halted: return "halted";
        case Outcome::Faulted: return "faulted";
        case Outcome::CycleLimit: return "cycle limit reached";
    }
    return "unknown";
}

}  // namespace rv
