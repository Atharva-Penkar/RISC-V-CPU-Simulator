#include "rv/decoder.hpp"

#include <array>
#include <charconv>

namespace rv {

std::optional<DecodedInst> decode(uint32_t word) {
    const uint32_t opcode = bits(word, 6, 0);
    const uint32_t funct3 = bits(word, 14, 12);
    const uint32_t funct7 = bits(word, 31, 25);
    for (const InstrSpec& spec : kIsa) {
        const Format format = formatOf(spec.kind);
        if (opcodeOf(spec.kind) != opcode
            || (hasFunct3(format) && spec.funct3 != funct3)
            || (hasFunct7(spec.kind) && spec.funct7 != funct7)
            || (spec.kind == Kind::System
                && (bits(word, 31, 20) != spec.funct12 || bits(word, 19, 15) != 0 || bits(word, 11, 7) != 0)))
            continue;
        DecodedInst inst{&spec, Reg::fromField(bits(word, 11, 7)), Reg::fromField(bits(word, 19, 15)),
                         Reg::fromField(bits(word, 24, 20)), 0};
        switch (format) {
            case Format::R:
                break;
            case Format::I:
                inst.rs2 = Reg();
                if (spec.kind == Kind::Shift)
                    inst.imm = static_cast<int32_t>(bits(word, 24, 20));
                else if (spec.kind != Kind::System)
                    inst.imm = signExtend(bits(word, 31, 20), 12);
                break;
            case Format::S:
                inst.rd = Reg();
                inst.imm = signExtend(bits(word, 31, 25) << 5 | bits(word, 11, 7), 12);
                break;
            case Format::B:
                inst.rd = Reg();
                inst.imm = signExtend(bits(word, 31, 31) << 12 | bits(word, 7, 7) << 11
                                    | bits(word, 30, 25) << 5 | bits(word, 11, 8) << 1, 13);
                break;
            case Format::U:
                inst.rs1 = inst.rs2 = Reg();
                inst.imm = static_cast<int32_t>(word & 0xFFFFF000u);
                break;
            case Format::J:
                inst.rs1 = inst.rs2 = Reg();
                inst.imm = signExtend(bits(word, 31, 31) << 20 | bits(word, 19, 12) << 12
                                    | bits(word, 20, 20) << 11 | bits(word, 30, 21) << 1, 21);
                break;
        }
        return inst;
    }
    return std::nullopt;
}

std::string toHex(uint32_t value, int digits) {
    std::array<char, 8> buffer{};
    auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, 16);
    const std::string_view hex(buffer.data(), static_cast<std::size_t>(end - buffer.data()));
    const std::size_t padding = hex.size() < static_cast<std::size_t>(digits) ? static_cast<std::size_t>(digits) - hex.size() : 0;
    return "0x" + std::string(padding, '0') + std::string(hex);
}

std::string disassemble(const DecodedInst& inst) {
    const std::string name(inst.spec->mnemonic);
    const std::string rd = toString(inst.rd);
    const std::string rs1 = toString(inst.rs1);
    const std::string rs2 = toString(inst.rs2);
    const std::string imm = std::to_string(inst.imm);
    const std::string relative = (inst.imm < 0 ? "pc" : "pc+") + imm;
    switch (inst.kind()) {
        case Kind::Alu: return name + " " + rd + ", " + rs1 + ", " + rs2;
        case Kind::AluImm:
        case Kind::Shift:
        case Kind::Jalr: return name + " " + rd + ", " + rs1 + ", " + imm;
        case Kind::Load: return name + " " + rd + ", " + imm + "[" + rs1 + "]";
        case Kind::Store: return name + " " + rs2 + ", " + imm + "[" + rs1 + "]";
        case Kind::Branch: return name + " " + rs1 + ", " + rs2 + ", " + relative;
        case Kind::Lui:
        case Kind::Auipc: return name + " " + rd + ", " + toHex(static_cast<uint32_t>(inst.imm) >> 12, 1);
        case Kind::Jal: return name + " " + rd + ", " + relative;
        case Kind::System: return name;
    }
    return name;
}

}  // namespace rv
