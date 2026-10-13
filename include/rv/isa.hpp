// RV32IM instruction set definition (the base integer set and the multiply/divide extension): registers, instruction kinds and the instruction table
// shared by the assembler, the decoder and the ALU control.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rv {

/*a general purpose register x0..x31; can only hold a valid index*/
class Reg {
public:
    constexpr Reg() = default;

    /*from a 5-bit instruction field*/
    static constexpr Reg fromField(uint32_t field) { return Reg(static_cast<uint8_t>(field & 31u)); }

    static constexpr std::optional<Reg> fromIndex(unsigned index) {
        if (index > 31)
            return std::nullopt;
        return Reg(static_cast<uint8_t>(index));
    }

    constexpr unsigned index() const { return index_; }
    constexpr bool operator==(const Reg&) const = default;

private:
    constexpr explicit Reg(uint8_t index) : index_(index) {}
    uint8_t index_ = 0;
};

inline std::string toString(Reg reg) { return "x" + std::to_string(reg.index()); }

/*instruction class: decides operand syntax, encoding format and control signals*/
enum class Kind : uint8_t { Alu, AluImm, Shift, Load, Store, Branch, Lui, Auipc, Jal, Jalr, System };

/*encoding layout*/
enum class Format : uint8_t { R, I, S, B, U, J };

/*branch comparisons return 1 when the condition holds; PassB forwards the second operand (LUI).
Mul..Remu are the M extension, computed by the multiplier and the divider instead of the ALU proper*/
enum class AluOp : uint8_t {
    Add, Sub, And, Or, Xor, Sll, Srl, Sra, Slt, Sltu, Eq, Ne, Lt, Ge, Ltu, Geu, PassB,
    Mul, Mulh, Mulhsu, Mulhu, Div, Divu, Rem, Remu,
};

/*the M extension's two functional units, which may take several cycles in EX*/
constexpr bool isMultiply(AluOp op) {
    return op == AluOp::Mul || op == AluOp::Mulh || op == AluOp::Mulhsu || op == AluOp::Mulhu;
}
constexpr bool isDivide(AluOp op) {
    return op == AluOp::Div || op == AluOp::Divu || op == AluOp::Rem || op == AluOp::Remu;
}

constexpr Format formatOf(Kind kind) {
    switch (kind) {
        case Kind::Alu: return Format::R;
        case Kind::AluImm:
        case Kind::Shift:
        case Kind::Load:
        case Kind::Jalr:
        case Kind::System: return Format::I;
        case Kind::Store: return Format::S;
        case Kind::Branch: return Format::B;
        case Kind::Lui:
        case Kind::Auipc: return Format::U;
        case Kind::Jal: return Format::J;
    }
    return Format::R;
}

constexpr uint32_t opcodeOf(Kind kind) {
    switch (kind) {
        case Kind::Alu: return 0b0110011;
        case Kind::AluImm:
        case Kind::Shift: return 0b0010011;
        case Kind::Load: return 0b0000011;
        case Kind::Store: return 0b0100011;
        case Kind::Branch: return 0b1100011;
        case Kind::Lui: return 0b0110111;
        case Kind::Auipc: return 0b0010111;
        case Kind::Jal: return 0b1101111;
        case Kind::Jalr: return 0b1100111;
        case Kind::System: return 0b1110011;
    }
    return 0;
}

constexpr bool hasFunct3(Format format) { return format != Format::U && format != Format::J; }
constexpr bool hasFunct7(Kind kind) { return kind == Kind::Alu || kind == Kind::Shift; }

struct InstrSpec {
    std::string_view mnemonic;
    Kind kind;
    uint32_t funct3;
    uint32_t funct7;    // R type, and imm[11:5] of the immediate shifts
    AluOp alu;
    uint32_t funct12 = 0;   // System: the whole immediate field (0 ECALL, 1 EBREAK)
};

/*single source of truth for the assembler, the decoder and the ALU control*/
inline constexpr auto kIsa = std::to_array<InstrSpec>({
    {"ADD",   Kind::Alu,    0b000, 0b0000000, AluOp::Add},
    {"SUB",   Kind::Alu,    0b000, 0b0100000, AluOp::Sub},
    {"SLL",   Kind::Alu,    0b001, 0b0000000, AluOp::Sll},
    {"SLT",   Kind::Alu,    0b010, 0b0000000, AluOp::Slt},
    {"SLTU",  Kind::Alu,    0b011, 0b0000000, AluOp::Sltu},
    {"XOR",   Kind::Alu,    0b100, 0b0000000, AluOp::Xor},
    {"SRL",   Kind::Alu,    0b101, 0b0000000, AluOp::Srl},
    {"SRA",   Kind::Alu,    0b101, 0b0100000, AluOp::Sra},
    {"OR",    Kind::Alu,    0b110, 0b0000000, AluOp::Or},
    {"AND",   Kind::Alu,    0b111, 0b0000000, AluOp::And},

    {"MUL",    Kind::Alu,   0b000, 0b0000001, AluOp::Mul},      // M extension: low 32 bits of the product
    {"MULH",   Kind::Alu,   0b001, 0b0000001, AluOp::Mulh},     // high 32 bits, signed x signed
    {"MULHSU", Kind::Alu,   0b010, 0b0000001, AluOp::Mulhsu},   // high 32 bits, signed x unsigned
    {"MULHU",  Kind::Alu,   0b011, 0b0000001, AluOp::Mulhu},    // high 32 bits, unsigned x unsigned
    {"DIV",    Kind::Alu,   0b100, 0b0000001, AluOp::Div},
    {"DIVU",   Kind::Alu,   0b101, 0b0000001, AluOp::Divu},
    {"REM",    Kind::Alu,   0b110, 0b0000001, AluOp::Rem},
    {"REMU",   Kind::Alu,   0b111, 0b0000001, AluOp::Remu},

    {"ADDI",  Kind::AluImm, 0b000, 0, AluOp::Add},
    {"SLTI",  Kind::AluImm, 0b010, 0, AluOp::Slt},
    {"SLTIU", Kind::AluImm, 0b011, 0, AluOp::Sltu},
    {"XORI",  Kind::AluImm, 0b100, 0, AluOp::Xor},
    {"ORI",   Kind::AluImm, 0b110, 0, AluOp::Or},
    {"ANDI",  Kind::AluImm, 0b111, 0, AluOp::And},

    {"SLLI",  Kind::Shift,  0b001, 0b0000000, AluOp::Sll},
    {"SRLI",  Kind::Shift,  0b101, 0b0000000, AluOp::Srl},
    {"SRAI",  Kind::Shift,  0b101, 0b0100000, AluOp::Sra},

    {"LB",    Kind::Load,   0b000, 0, AluOp::Add},
    {"LH",    Kind::Load,   0b001, 0, AluOp::Add},
    {"LW",    Kind::Load,   0b010, 0, AluOp::Add},
    {"LBU",   Kind::Load,   0b100, 0, AluOp::Add},
    {"LHU",   Kind::Load,   0b101, 0, AluOp::Add},

    {"SB",    Kind::Store,  0b000, 0, AluOp::Add},
    {"SH",    Kind::Store,  0b001, 0, AluOp::Add},
    {"SW",    Kind::Store,  0b010, 0, AluOp::Add},

    {"BEQ",   Kind::Branch, 0b000, 0, AluOp::Eq},
    {"BNE",   Kind::Branch, 0b001, 0, AluOp::Ne},
    {"BLT",   Kind::Branch, 0b100, 0, AluOp::Lt},
    {"BGE",   Kind::Branch, 0b101, 0, AluOp::Ge},
    {"BLTU",  Kind::Branch, 0b110, 0, AluOp::Ltu},
    {"BGEU",  Kind::Branch, 0b111, 0, AluOp::Geu},

    {"LUI",   Kind::Lui,    0, 0, AluOp::PassB},
    {"AUIPC", Kind::Auipc,  0, 0, AluOp::Add},
    {"JAL",   Kind::Jal,    0, 0, AluOp::Add},       // ALU computes the target pc+imm
    {"JALR",  Kind::Jalr,   0b000, 0, AluOp::Add},   // ALU computes the target rs1+imm

    {"ECALL",  Kind::System, 0b000, 0, AluOp::PassB, 0},     // both halt the simulation
    {"EBREAK", Kind::System, 0b000, 0, AluOp::PassB, 1},
});

/*mnemonic must be upper case*/
constexpr const InstrSpec* findSpec(std::string_view mnemonic) {
    for (const InstrSpec& spec : kIsa)
        if (spec.mnemonic == mnemonic)
            return &spec;
    return nullptr;
}

/*an instruction with its fields extracted; registers a format does not use are x0*/
struct DecodedInst {
    const InstrSpec* spec = nullptr;
    Reg rd;
    Reg rs1;
    Reg rs2;
    int32_t imm = 0;    // sign-extended; U type holds the value already shifted left by 12

    constexpr Kind kind() const { return spec->kind; }
};

/*value[hi:lo]*/
constexpr uint32_t bits(uint32_t value, unsigned hi, unsigned lo) {
    return (value >> lo) & ((1u << (hi - lo + 1)) - 1);
}

constexpr int32_t signExtend(uint32_t value, unsigned width) {
    const uint32_t sign = 1u << (width - 1);
    value &= (sign << 1) - 1;
    return static_cast<int32_t>((value ^ sign) - sign);
}

}  // namespace rv
