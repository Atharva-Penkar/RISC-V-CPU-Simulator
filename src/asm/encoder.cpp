#include "rv/assembler.hpp"

namespace rv {

uint32_t encode(const DecodedInst& inst) {
    const InstrSpec& spec = *inst.spec;
    const auto imm = static_cast<uint32_t>(inst.imm);
    const uint32_t opcode = opcodeOf(spec.kind);
    const uint32_t funct3 = spec.funct3 << 12;
    const uint32_t rd = inst.rd.index() << 7;
    const uint32_t rs1 = inst.rs1.index() << 15;
    const uint32_t rs2 = inst.rs2.index() << 20;
    switch (formatOf(spec.kind)) {
        case Format::R:
            return spec.funct7 << 25 | rs2 | rs1 | funct3 | rd | opcode;
        case Format::I: {
            const uint32_t field = (spec.kind == Kind::Shift) ? (spec.funct7 << 5 | bits(imm, 4, 0)) : bits(imm, 11, 0);
            return field << 20 | rs1 | funct3 | rd | opcode;
        }
        case Format::S:
            return bits(imm, 11, 5) << 25 | rs2 | rs1 | funct3 | bits(imm, 4, 0) << 7 | opcode;
        case Format::B:
            return bits(imm, 12, 12) << 31 | bits(imm, 10, 5) << 25 | rs2 | rs1 | funct3
                 | bits(imm, 4, 1) << 8 | bits(imm, 11, 11) << 7 | opcode;
        case Format::U:
            return (imm & 0xFFFFF000u) | rd | opcode;
        case Format::J:
            return bits(imm, 20, 20) << 31 | bits(imm, 10, 1) << 21 | bits(imm, 11, 11) << 20
                 | bits(imm, 19, 12) << 12 | rd | opcode;
    }
    return 0;
}

}  // namespace rv
