// RISC-V RV32I assembler and 5-stage pipelined CPU simulator.
// Build: g++ -std=c++20 -O2 22cs02011_assm_and_cpu.cpp -o 22cs02011_assm_and_cpu
#include <algorithm>
#include <array>
#include <bitset>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>
#include <version>
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#include <expected>
#endif

namespace rv {

constexpr std::string_view kUnderline = "\033[4m";
constexpr std::string_view kNoUnderline = "\033[0m";
constexpr std::size_t kMemoryBytes = 256;   // 64 words of data memory
constexpr std::size_t kMaxCycles = 10000;   // guards against programs that never terminate

/*---------------------------------------------------------------------------------------------
  Result: std::expected when the standard library has it (C++23), otherwise a minimal stand-in
  with the same interface (has_value, operator bool, operator*, operator->, error).
---------------------------------------------------------------------------------------------*/
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
template <class T, class E>
using Result = std::expected<T, E>;

template <class E>
std::unexpected<E> fail(E error) { return std::unexpected<E>(std::move(error)); }
#else
template <class E>
struct Failure { E error; };

template <class E>
Failure<E> fail(E error) { return {std::move(error)}; }

template <class T, class E>
class Result {
public:
    Result(T value) : value_(std::in_place_index<0>, std::move(value)) {}
    template <class F>
    Result(Failure<F> failure) : value_(std::in_place_index<1>, E(std::move(failure.error))) {}

    bool has_value() const { return value_.index() == 0; }
    explicit operator bool() const { return has_value(); }
    T& operator*() { return std::get<0>(value_); }
    const T& operator*() const { return std::get<0>(value_); }
    T* operator->() { return &std::get<0>(value_); }
    const T* operator->() const { return &std::get<0>(value_); }
    const E& error() const { return std::get<1>(value_); }

private:
    std::variant<T, E> value_;
};
#endif

/*1-based position in the assembly source*/
struct Diagnostic {
    std::size_t line = 0;
    std::size_t column = 0;
    std::string message;
};

template <class... Parts>
std::string concat(const Parts&... parts) {
    std::string text;
    ((text += parts), ...);
    return text;
}

std::string toHex(uint32_t value) {
    std::array<char, 8> digits{};
    auto [end, ec] = std::to_chars(digits.data(), digits.data() + digits.size(), value, 16);
    return concat("0x", std::string_view(digits.data(), static_cast<std::size_t>(end - digits.data())));
}

/*---------------------------------------------------------------------------------------------
  ISA definition
---------------------------------------------------------------------------------------------*/

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

std::string toString(Reg reg) { return concat("x", std::to_string(reg.index())); }

/*instruction class: decides operand syntax, encoding format and control signals*/
enum class Kind : uint8_t { Alu, AluImm, Shift, Load, Store, Branch, Lui, Auipc, Jal, Jalr };

/*encoding layout*/
enum class Format : uint8_t { R, I, S, B, U, J };

enum class AluOp : uint8_t { Add, Sub, And, Or, Xor, Sll, Srl, Sra, Slt, Sltu, Eq, Ne, Lt, Ge, Ltu, Geu, PassB };

constexpr Format formatOf(Kind kind) {
    switch (kind) {
        case Kind::Alu: return Format::R;
        case Kind::AluImm:
        case Kind::Shift:
        case Kind::Load:
        case Kind::Jalr: return Format::I;
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
};

/*single source of truth for the assembler, the decoder and the ALU control*/
constexpr auto kIsa = std::to_array<InstrSpec>({
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
});

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

constexpr uint32_t bits(uint32_t value, unsigned hi, unsigned lo) {
    return (value >> lo) & ((1u << (hi - lo + 1)) - 1);
}

constexpr int32_t signExtend(uint32_t value, unsigned width) {
    const uint32_t sign = 1u << (width - 1);
    value &= (sign << 1) - 1;
    return static_cast<int32_t>((value ^ sign) - sign);
}

constexpr uint32_t encode(const DecodedInst& inst) {
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

constexpr std::optional<DecodedInst> decode(uint32_t word) {
    const uint32_t opcode = bits(word, 6, 0);
    const uint32_t funct3 = bits(word, 14, 12);
    const uint32_t funct7 = bits(word, 31, 25);
    for (const InstrSpec& spec : kIsa) {
        const Format format = formatOf(spec.kind);
        if (opcodeOf(spec.kind) != opcode
            || (hasFunct3(format) && spec.funct3 != funct3)
            || (hasFunct7(spec.kind) && spec.funct7 != funct7))
            continue;
        DecodedInst inst{&spec, Reg::fromField(bits(word, 11, 7)), Reg::fromField(bits(word, 19, 15)),
                         Reg::fromField(bits(word, 24, 20)), 0};
        switch (format) {
            case Format::R:
                break;
            case Format::I:
                inst.rs2 = Reg();
                inst.imm = (spec.kind == Kind::Shift) ? static_cast<int32_t>(bits(word, 24, 20))
                                                      : signExtend(bits(word, 31, 20), 12);
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

/*compile-time checks against encodings produced by the LLVM RISC-V assembler*/
constexpr uint32_t encodeChecked(std::string_view mnemonic, unsigned rd, unsigned rs1, unsigned rs2, int32_t imm) {
    return encode({findSpec(mnemonic), Reg::fromField(rd), Reg::fromField(rs1), Reg::fromField(rs2), imm});
}
static_assert(encodeChecked("ADD", 1, 2, 3, 0) == 0x003100B3);
static_assert(encodeChecked("SRAI", 4, 3, 0, 4) == 0x4041D213);
static_assert(encodeChecked("SW", 0, 1, 4, -8) == 0xFE40AC23);
static_assert(encodeChecked("BNE", 0, 1, 2, -64) == 0xFC2090E3);
static_assert(encodeChecked("JAL", 1, 0, 0, -72) == 0xFB9FF0EF);
static_assert(encodeChecked("LUI", 7, 0, 0, static_cast<int32_t>(0x80000000u)) == 0x800003B7);
static_assert(decode(0xFC2090E3)->imm == -64 && decode(0xFC2090E3)->spec == findSpec("BNE"));
static_assert(decode(0x4041D213)->spec == findSpec("SRAI") && decode(0x4041D213)->imm == 4);

std::string disassemble(const DecodedInst& inst) {
    const std::string name(inst.spec->mnemonic);
    const std::string imm = std::to_string(inst.imm);
    switch (inst.kind()) {
        case Kind::Alu:
            return concat(name, " ", toString(inst.rd), ", ", toString(inst.rs1), ", ", toString(inst.rs2));
        case Kind::AluImm:
        case Kind::Shift:
        case Kind::Jalr:
            return concat(name, " ", toString(inst.rd), ", ", toString(inst.rs1), ", ", imm);
        case Kind::Load:
            return concat(name, " ", toString(inst.rd), ", ", imm, "[", toString(inst.rs1), "]");
        case Kind::Store:
            return concat(name, " ", toString(inst.rs2), ", ", imm, "[", toString(inst.rs1), "]");
        case Kind::Branch:
            return concat(name, " ", toString(inst.rs1), ", ", toString(inst.rs2), ", pc", inst.imm < 0 ? "" : "+", imm);
        case Kind::Lui:
        case Kind::Auipc:
            return concat(name, " ", toString(inst.rd), ", ", toHex(static_cast<uint32_t>(inst.imm) >> 12));
        case Kind::Jal:
            return concat(name, " ", toString(inst.rd), ", pc", inst.imm < 0 ? "" : "+", imm);
    }
    return name;
}

/*---------------------------------------------------------------------------------------------
  Assembler
---------------------------------------------------------------------------------------------*/
class Assembler {
public:
    using Program = std::vector<uint32_t>;

    /*assembles the whole source; on failure returns every error found, in source order*/
    Result<Program, std::vector<Diagnostic>> assemble(std::span<const std::string> lines) {
        source_.assign(lines.begin(), lines.end());
        for (std::string& line : source_)
            std::transform(line.begin(), line.end(), line.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        labels_.clear();

        const std::vector<Statement> statements = split();
        std::vector<Diagnostic> diagnostics;
        uint32_t index = 0;
        for (const Statement& statement : statements) {
            if (statement.label) {
                if (auto error = defineLabel(*statement.label, index))
                    diagnostics.push_back(*error);
            }
            if (!statement.words.empty())
                index++;
        }

        Program program;
        for (const Statement& statement : statements) {
            if (statement.words.empty())
                continue;
            auto word = encodeStatement(statement, static_cast<uint32_t>(program.size()));
            if (word)
                program.push_back(*word);
            else {
                diagnostics.push_back(word.error());
                program.push_back(0);   // keeps instruction indices aligned for later statements
            }
        }
        if (!diagnostics.empty()) {
            std::stable_sort(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& a, const Diagnostic& b) {
                return std::pair{a.line, a.column} < std::pair{b.line, b.column};
            });
            return fail(std::move(diagnostics));
        }
        return program;
    }

private:
    struct Token {
        std::string_view text;
        std::size_t line;
        std::size_t column;
    };

    struct Statement {
        std::optional<Token> label;
        std::vector<Token> words;   // mnemonic followed by operands
    };

    static bool isSeparator(char c) { return c == ' ' || c == '\t' || c == ',' || c == '\r'; }

    static Diagnostic diagnose(const Token& token, std::string message) {
        return {token.line, token.column, std::move(message)};
    }

    /*statements end at ';' or at the end of a line; "LABEL:" may stand alone or precede an instruction*/
    std::vector<Statement> split() const {
        std::vector<Statement> statements;
        for (std::size_t lineIndex = 0; lineIndex < source_.size(); lineIndex++) {
            const std::string_view line = source_[lineIndex];
            Statement current;
            auto finish = [&] {
                if (current.label || !current.words.empty())
                    statements.push_back(std::move(current));
                current = Statement{};
            };
            std::size_t i = 0;
            while (i < line.size()) {
                if (line[i] == ';') {
                    finish();
                    i++;
                    continue;
                }
                if (isSeparator(line[i])) {
                    i++;
                    continue;
                }
                const std::size_t start = i;
                while (i < line.size() && !isSeparator(line[i]) && line[i] != ';')
                    i++;
                Token token{line.substr(start, i - start), lineIndex + 1, start + 1};
                if (token.text.back() == ':' && !current.label && current.words.empty()) {
                    token.text.remove_suffix(1);
                    current.label = token;
                }
                else
                    current.words.push_back(token);
            }
            finish();
        }
        return statements;
    }

    std::optional<Diagnostic> defineLabel(const Token& label, uint32_t index) {
        const std::string_view name = label.text;
        auto validChar = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.'; };
        if (name.empty() || std::isdigit(static_cast<unsigned char>(name[0])) || !std::all_of(name.begin(), name.end(), validChar))
            return diagnose(label, concat("invalid label name '", name, "'"));
        if (!labels_.emplace(std::string(name), index).second)
            return diagnose(label, concat("label '", name, "' is defined more than once"));
        return std::nullopt;
    }

    Result<Reg, Diagnostic> parseRegister(const Token& token) const {
        const std::string_view text = token.text;
        unsigned index = 0;
        if (text.size() >= 2 && text[0] == 'X') {
            auto [end, ec] = std::from_chars(text.data() + 1, text.data() + text.size(), index);
            if (ec == std::errc{} && end == text.data() + text.size() && text.size() <= 3)
                if (auto reg = Reg::fromIndex(index))
                    return *reg;
        }
        return fail(diagnose(token, concat("invalid register '", text, "' (registers are x0 to x31)")));
    }

    /*hexadecimal 0x... (up to 8 digits, 32-bit two's complement, so 0xFFFFFFFE = -2) or signed decimal*/
    Result<int64_t, Diagnostic> parseImmediate(const Token& token) const {
        std::string_view text = token.text;
        bool negative = false;
        if (!text.empty() && (text[0] == '-' || text[0] == '+')) {
            negative = text[0] == '-';
            text.remove_prefix(1);
        }
        const bool hex = text.size() > 2 && text[0] == '0' && text[1] == 'X';
        if (hex)
            text.remove_prefix(2);
        uint64_t magnitude = 0;
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), magnitude, hex ? 16 : 10);
        if (text.empty() || ec != std::errc{} || end != text.data() + text.size() || (hex && text.size() > 8)
            || magnitude > 0xFFFFFFFFull)
            return fail(diagnose(token, concat("invalid immediate '", token.text, "'")));
        const int64_t value = hex ? static_cast<int32_t>(static_cast<uint32_t>(magnitude)) : static_cast<int64_t>(magnitude);
        return negative ? -value : value;
    }

    /*an immediate that must fit a field of the given width, as a signed or an unsigned bit pattern*/
    Result<int32_t, Diagnostic> parseField(const Token& token, unsigned width) const {
        auto value = parseImmediate(token);
        if (!value)
            return fail(value.error());
        if (*value < -(int64_t{1} << (width - 1)) || *value >= (int64_t{1} << width))
            return fail(diagnose(token, concat("immediate '", token.text, "' does not fit in ", std::to_string(width), " bits")));
        return signExtend(static_cast<uint32_t>(*value), width);
    }

    /*PC-relative byte offset to a label, or a numeric byte offset*/
    Result<int32_t, Diagnostic> parseOffset(const Token& token, uint32_t index, unsigned width) const {
        int64_t offset = 0;
        if (auto label = labels_.find(token.text); label != labels_.end())
            offset = (static_cast<int64_t>(label->second) - index) * 4;
        else if (std::isdigit(static_cast<unsigned char>(token.text[0])) || token.text[0] == '-' || token.text[0] == '+') {
            auto value = parseImmediate(token);
            if (!value)
                return fail(value.error());
            offset = *value;
        }
        else
            return fail(diagnose(token, concat("undefined label '", token.text, "'")));
        if (offset % 2 != 0)
            return fail(diagnose(token, "branch/jump offset must be even"));
        if (offset < -(int64_t{1} << (width - 1)) || offset >= (int64_t{1} << (width - 1)))
            return fail(diagnose(token, concat("target '", token.text, "' is out of range")));
        return static_cast<int32_t>(offset);
    }

    /*imm[xN] or imm(xN); an empty immediate means 0*/
    Result<std::pair<int32_t, Reg>, Diagnostic> parseMemory(const Token& token) const {
        const std::string_view text = token.text;
        const std::size_t open = text.find_first_of("[(");
        const char close = (open != std::string_view::npos && text[open] == '[') ? ']' : ')';
        if (open == std::string_view::npos || text.back() != close)
            return fail(diagnose(token, concat("invalid memory operand '", text, "' (expected immediate[register])")));
        int32_t offset = 0;
        if (open > 0) {
            auto value = parseField({text.substr(0, open), token.line, token.column}, 12);
            if (!value)
                return fail(value.error());
            offset = *value;
        }
        auto base = parseRegister({text.substr(open + 1, text.size() - open - 2), token.line, token.column + open + 1});
        if (!base)
            return fail(base.error());
        return std::pair{offset, *base};
    }

    Result<uint32_t, Diagnostic> encodeStatement(const Statement& statement, uint32_t index) const {
        const Token& mnemonic = statement.words[0];
        const InstrSpec* spec = findSpec(mnemonic.text);
        if (!spec)
            return fail(diagnose(mnemonic, concat("unknown instruction '", mnemonic.text, "'")));
        const std::span<const Token> operands = std::span(statement.words).subspan(1);
        const std::size_t expected = (spec->kind == Kind::Load || spec->kind == Kind::Store || formatOf(spec->kind) == Format::U
                                      || spec->kind == Kind::Jal) ? 2 : 3;
        if (operands.size() != expected)
            return fail(diagnose(mnemonic, concat(spec->mnemonic, " expects ", std::to_string(expected), " operands, got ",
                                                  std::to_string(operands.size()))));

        DecodedInst inst{spec, Reg(), Reg(), Reg(), 0};
        auto reg = [&](std::size_t i, Reg& out) -> std::optional<Diagnostic> {
            auto value = parseRegister(operands[i]);
            if (!value)
                return value.error();
            out = *value;
            return std::nullopt;
        };
        auto immediate = [&](Result<int32_t, Diagnostic> value) -> std::optional<Diagnostic> {
            if (!value)
                return value.error();
            inst.imm = *value;
            return std::nullopt;
        };

        std::optional<Diagnostic> error;
        switch (spec->kind) {
            case Kind::Alu:
                error = reg(0, inst.rd);
                if (!error) error = reg(1, inst.rs1);
                if (!error) error = reg(2, inst.rs2);
                break;
            case Kind::AluImm:
                error = reg(0, inst.rd);
                if (!error) error = reg(1, inst.rs1);
                if (!error) error = immediate(parseField(operands[2], 12));
                break;
            case Kind::Shift: {
                error = reg(0, inst.rd);
                if (!error) error = reg(1, inst.rs1);
                if (error)
                    break;
                auto shamt = parseImmediate(operands[2]);
                if (!shamt)
                    error = shamt.error();
                else if (*shamt < 0 || *shamt > 31)
                    error = diagnose(operands[2], "shift amount must be between 0 and 31");
                else
                    inst.imm = static_cast<int32_t>(*shamt);
                break;
            }
            case Kind::Load:
            case Kind::Store: {
                error = reg(0, spec->kind == Kind::Load ? inst.rd : inst.rs2);
                if (error)
                    break;
                auto memory = parseMemory(operands[1]);
                if (!memory)
                    error = memory.error();
                else
                    std::tie(inst.imm, inst.rs1) = *memory;
                break;
            }
            case Kind::Branch:
                error = reg(0, inst.rs1);
                if (!error) error = reg(1, inst.rs2);
                if (!error) error = immediate(parseOffset(operands[2], index, 13));
                break;
            case Kind::Lui:
            case Kind::Auipc: {
                error = reg(0, inst.rd);
                if (error)
                    break;
                auto upper = parseField(operands[1], 20);
                if (!upper)
                    error = upper.error();
                else
                    inst.imm = static_cast<int32_t>(static_cast<uint32_t>(*upper) << 12);
                break;
            }
            case Kind::Jal:
                error = reg(0, inst.rd);
                if (!error) error = immediate(parseOffset(operands[1], index, 21));
                break;
            case Kind::Jalr:
                /*jalr rd,rs1,imm|label  -  a label gives its absolute byte address*/
                error = reg(0, inst.rd);
                if (!error) error = reg(1, inst.rs1);
                if (error)
                    break;
                if (auto label = labels_.find(operands[2].text); label != labels_.end()) {
                    const int64_t address = int64_t{label->second} * 4;
                    if (address >= 2048)
                        error = diagnose(operands[2], concat("label '", operands[2].text, "' is out of range for JALR"));
                    else
                        inst.imm = static_cast<int32_t>(address);
                }
                else
                    error = immediate(parseField(operands[2], 12));
                break;
        }
        if (error)
            return fail(*error);
        return encode(inst);
    }

    std::vector<std::string> source_;                     // upper-cased copy; tokens view into it
    std::map<std::string, uint32_t, std::less<>> labels_;  // label -> instruction index
};

/*---------------------------------------------------------------------------------------------
  CPU
---------------------------------------------------------------------------------------------*/
struct Control {
    bool readsRs1 = false;
    bool readsRs2 = false;
    bool aluSrc = false;    // second ALU operand is the immediate
    bool regWrite = false;
    bool memRead = false;
    bool memWrite = false;
    bool memToReg = false;
    bool branch = false;
    bool jump = false;
};

constexpr Control controlFor(Kind kind) {
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
    auto add = [&](bool set, std::string_view name) {
        if (set)
            flags += concat(flags.empty() ? "" : " ", name);
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
    return concat("[", flags, "]");
}

/*branch ops return 1 when the condition holds; signed views only where the ISA needs them*/
constexpr uint32_t aluExecute(AluOp op, uint32_t a, uint32_t b) {
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

/*register file with a scoreboard: a register is busy while an issued instruction will still write it*/
class RegisterFile {
public:
    uint32_t read(Reg reg) const { return values_[reg.index()]; }

    /*the only write port; x0 is hard-wired to zero*/
    void write(Reg reg, uint32_t value) {
        if (reg.index() != 0)
            values_[reg.index()] = value;
    }

    bool busy(Reg reg) const { return pending_[reg.index()].has_value(); }

    void reserve(Reg reg, uint64_t tag) {
        if (reg.index() != 0)
            pending_[reg.index()] = tag;
    }

    /*only the youngest writer clears the reservation*/
    void release(Reg reg, uint64_t tag) {
        if (pending_[reg.index()] == tag)
            pending_[reg.index()].reset();
    }

private:
    std::array<uint32_t, 32> values_{};
    std::array<std::optional<uint64_t>, 32> pending_{};
};

/*byte-addressed, little-endian; accesses must be naturally aligned*/
class DataMemory {
public:
    explicit DataMemory(std::size_t bytes) : bytes_(bytes, 0) {}

    std::size_t size() const { return bytes_.size(); }

    /*zero-extended value, or nullopt for an invalid access*/
    std::optional<uint32_t> load(uint32_t address, uint32_t width) const {
        if (!valid(address, width))
            return std::nullopt;
        uint32_t value = 0;
        for (uint32_t i = width; i-- > 0;)
            value = value << 8 | bytes_[address + i];
        return value;
    }

    bool store(uint32_t address, uint32_t width, uint32_t value) {
        if (!valid(address, width))
            return false;
        for (uint32_t i = 0; i < width; i++)
            bytes_[address + i] = static_cast<uint8_t>(value >> (8 * i));
        return true;
    }

private:
    bool valid(uint32_t address, uint32_t width) const {
        return address % width == 0 && address < bytes_.size() && bytes_.size() - address >= width;
    }

    std::vector<uint8_t> bytes_;
};

class Cpu {
public:
    /*program must outlive the Cpu*/
    Cpu(std::span<const uint32_t> program, std::ostream& out)
        : imem_(program), dmem_(kMemoryBytes), out_(out) {
        pc_.valid = inProgram(0);
    }

    void run() {
        std::size_t cycle = 0;
        while (pc_.valid || ifid_.valid || idex_.valid || exmem_.valid || memwb_.valid) {
            if (cycle == kMaxCycles) {
                out_ << "\nStopped after " << kMaxCycles << " cycles (possible infinite loop).\n\n";
                break;
            }
            cycle++;
            out_ << kUnderline << "\nPASS: " << cycle << kNoUnderline << "\n\n";
            /*stages run in reverse so each one consumes its input latch before the previous stage refills it*/
            writeBack();
            memoryAccess();
            execute();
            decodeStage();
            fetch();
            printPipeline();
        }
        out_ << kUnderline << "\nEND OF CPU OPERATION" << kNoUnderline << "\n\n";
    }

    void printRegisters() const {
        out_ << kUnderline << "\nREGISTER FILE" << kNoUnderline << "\n\n";
        for (unsigned i = 0; i < 16; i++) {
            out_ << "Register " << i << ": " << static_cast<int32_t>(regs_.read(Reg::fromField(i)))
                 << "\t\t\t\tRegister " << i + 16 << ": " << static_cast<int32_t>(regs_.read(Reg::fromField(i + 16))) << '\n';
        }
    }

    void printMemory() const {
        out_ << kUnderline << "\nDATA MEMORY" << kNoUnderline << "\n\n";
        const std::size_t words = dmem_.size() / 4;
        const std::size_t rows = words / 4;
        for (std::size_t row = 0; row < rows; row++) {
            for (std::size_t column = 0; column < 4; column++) {
                const auto address = static_cast<uint32_t>((row + rows * column) * 4);
                out_ << "Address " << address << ": " << static_cast<int32_t>(dmem_.load(address, 4).value_or(0))
                     << (column == 3 ? "\n" : "\t\t");
            }
        }
    }

private:
    struct ProgramCounter {
        uint32_t value = 0;
        bool valid = false;
    };

    struct IfId {
        bool valid = false;
        bool stall = false;
        uint32_t pc = 0;
        uint32_t ir = 0;
    };

    struct IdEx {
        bool valid = false;
        DecodedInst inst;
        Control cw;
        uint32_t pc = 0;
        uint32_t rs1 = 0;   // operand values
        uint32_t rs2 = 0;
        uint64_t tag = 0;
    };

    struct ExMem {
        bool valid = false;
        DecodedInst inst;
        Control cw;
        uint32_t aluOut = 0;    // memory address, or the value to write back
        uint32_t storeData = 0;
        uint64_t tag = 0;
    };

    struct MemWb {
        bool valid = false;
        DecodedInst inst;
        Control cw;
        uint32_t aluOut = 0;
        uint32_t loadData = 0;
        uint64_t tag = 0;
    };

    bool inProgram(uint32_t address) const { return address % 4 == 0 && address / 4 < imem_.size(); }

    void fetch() {
        if (ifid_.valid || !pc_.valid)   // a stalled instruction still occupies IF/ID
            return;
        ifid_ = {true, false, pc_.value, imem_[pc_.value / 4]};
        pc_.value += 4;
        pc_.valid = inProgram(pc_.value);
    }

    void decodeStage() {
        if (!ifid_.valid)
            return;
        const std::optional<DecodedInst> inst = decode(ifid_.ir);
        if (!inst) {
            out_ << "\nUnknown instruction " << toHex(ifid_.ir) << " at address " << ifid_.pc << " is skipped.\n\n";
            ifid_.valid = false;
            return;
        }
        const Control cw = controlFor(inst->kind());
        /*data hazard: wait until the pending write back of a source register has happened*/
        if ((cw.readsRs1 && regs_.busy(inst->rs1)) || (cw.readsRs2 && regs_.busy(inst->rs2))) {
            ifid_.stall = true;
            return;
        }
        idex_ = {true, *inst, cw, ifid_.pc, regs_.read(inst->rs1), regs_.read(inst->rs2), nextTag_++};
        if (cw.regWrite)
            regs_.reserve(inst->rd, idex_.tag);
        ifid_.valid = false;
        ifid_.stall = false;
    }

    void execute() {
        if (!idex_.valid)
            return;
        const DecodedInst& inst = idex_.inst;
        const Control& cw = idex_.cw;
        const bool pcRelative = inst.kind() == Kind::Auipc || inst.kind() == Kind::Jal;
        const uint32_t a = pcRelative ? idex_.pc : idex_.rs1;
        const uint32_t b = cw.aluSrc ? static_cast<uint32_t>(inst.imm) : idex_.rs2;
        const uint32_t aluOut = aluExecute(inst.spec->alu, a, b);

        exmem_ = {true, inst, cw, cw.jump ? idex_.pc + 4 : aluOut, idex_.rs2, idex_.tag};
        idex_.valid = false;

        std::optional<uint32_t> target;
        if (cw.branch && aluOut != 0)
            target = idex_.pc + static_cast<uint32_t>(inst.imm);
        else if (cw.jump)
            target = aluOut & ~1u;
        if (target) {
            pc_.value = *target;
            pc_.valid = inProgram(*target);
            if (!pc_.valid && *target != 4 * imem_.size())
                out_ << "\nJump/branch target " << *target << " is outside the program. Fetching stops.\n\n";
            /*the instruction fetched after the branch is on the wrong path; it has not been decoded yet*/
            ifid_ = IfId{};
        }
    }

    void memoryAccess() {
        if (!exmem_.valid)
            return;
        const Control& cw = exmem_.cw;
        uint32_t loadData = 0;
        if (cw.memRead || cw.memWrite) {
            const uint32_t funct3 = exmem_.inst.spec->funct3;
            const uint32_t width = 1u << (funct3 & 3u);
            const uint32_t address = exmem_.aluOut;
            bool ok = true;
            if (cw.memWrite)
                ok = dmem_.store(address, width, exmem_.storeData);
            else if (auto value = dmem_.load(address, width)) {
                const bool zeroExtend = (funct3 & 4u) != 0 || width == 4;   // LBU, LHU
                loadData = zeroExtend ? *value : static_cast<uint32_t>(signExtend(*value, 8 * width));
            }
            else
                ok = false;
            if (!ok)
                out_ << "\nInvalid memory access at address " << address << " is ignored.\n\n";
        }
        memwb_ = {true, exmem_.inst, cw, exmem_.aluOut, loadData, exmem_.tag};
        exmem_.valid = false;
    }

    void writeBack() {
        if (!memwb_.valid)
            return;
        if (memwb_.cw.regWrite) {
            regs_.write(memwb_.inst.rd, memwb_.cw.memToReg ? memwb_.loadData : memwb_.aluOut);
            regs_.release(memwb_.inst.rd, memwb_.tag);
        }
        memwb_.valid = false;
    }

    void printPipeline() const {
        auto heading = [&](std::string_view name) { out_ << kUnderline << name << kNoUnderline << '\n'; };
        heading("MEMORY OPERATION WRITE BACK");
        out_ << "valid: " << memwb_.valid;
        if (memwb_.valid)
            out_ << "  " << disassemble(memwb_.inst) << "  aluout: " << static_cast<int32_t>(memwb_.aluOut)
                 << "  ldout: " << static_cast<int32_t>(memwb_.loadData) << "  " << toString(memwb_.cw);
        heading("\nEXECUTE MEMORY OPERATION");
        out_ << "valid: " << exmem_.valid;
        if (exmem_.valid)
            out_ << "  " << disassemble(exmem_.inst) << "  aluout: " << static_cast<int32_t>(exmem_.aluOut)
                 << "  rs2: " << static_cast<int32_t>(exmem_.storeData) << "  " << toString(exmem_.cw);
        heading("\nINSTRUCTION DECODE EXECUTE");
        out_ << "valid: " << idex_.valid;
        if (idex_.valid)
            out_ << "  " << disassemble(idex_.inst) << "  dpc: " << idex_.pc << "  rs1: " << static_cast<int32_t>(idex_.rs1)
                 << "  rs2: " << static_cast<int32_t>(idex_.rs2) << "  imm: " << idex_.inst.imm << "  " << toString(idex_.cw);
        heading("\nINSTRUCTION FETCH INSTRUCTION DECODE");
        out_ << "valid: " << ifid_.valid << "  stall: " << ifid_.stall;
        if (ifid_.valid)
            out_ << "  dpc: " << ifid_.pc << "  ir: " << std::bitset<32>(ifid_.ir);
        heading("\nPROGRAM COUNTER");
        out_ << "value: " << pc_.value << "  valid: " << pc_.valid << '\n';
    }

    std::span<const uint32_t> imem_;
    RegisterFile regs_;
    DataMemory dmem_;
    std::ostream& out_;
    ProgramCounter pc_;
    IfId ifid_;
    IdEx idex_;
    ExMem exmem_;
    MemWb memwb_;
    uint64_t nextTag_ = 0;
};

}  // namespace rv

namespace {

void printBanner() {
    using rv::kNoUnderline;
    using rv::kUnderline;
    std::cout << kUnderline << "\n\n\nATHARVA ATUL PENKAR\n22CS02011\nCOMPUTER ORGANIZATION AND ARCHITECTURE LABORATORY\nRISCV ASSEMBLER CODE"
              << kNoUnderline << '\n'
              << kUnderline << "\nRULES FOR WRITING ASSEMBLY INSTRUCTIONS\n" << kNoUnderline << '\n'
              << "1.\tInstructions are separated by semi-colons (;) or new lines. Finish the input with an empty line.\n"
              << "2.\tLabels are written as \"loop:;\" or in front of an instruction: \"L2: add x1,x2,x3;\"\n"
              << "3.\tImmediates are hexadecimal (0xF, 0x0000000F) or decimal (15, -2).\n"
              << "\tHexadecimal values are 32-bit two's complement: 0xFFFFFFFE = -2\n"
              << "4.\tGeneral purpose registers are written as x0 to x31\n"
              << "5.\tJump and branch targets are labels, e.g. beq x1,x2,loop; jal x1,label;\n"
              << "6.\tMemory operands are written immediate[register], e.g. lw x1,0x4[x5];\n"
              << "7.\tlui/auipc take the 20-bit upper immediate, e.g. lui x1,0x12345;\n";
}

void printDiagnostics(const std::vector<rv::Diagnostic>& diagnostics, const std::vector<std::string>& lines) {
    for (const rv::Diagnostic& d : diagnostics) {
        std::cout << d.line << ':' << d.column << ": error: " << d.message << '\n'
                  << "    " << lines[d.line - 1] << '\n'
                  << "    " << std::string(d.column - 1, ' ') << "^\n";
    }
    std::cout << diagnostics.size() << (diagnostics.size() == 1 ? " error" : " errors") << ". Assembly failed.\n";
}

}  // namespace

int main() {
    printBanner();
    std::cout << rv::kUnderline << "\nENTER ASSEMBLY INSTRUCTIONS:\n" << rv::kNoUnderline << std::endl;
    std::vector<std::string> lines;
    for (std::string line; std::getline(std::cin, line) && !line.empty();)
        lines.push_back(line);

    rv::Assembler assembler;
    const auto program = assembler.assemble(lines);
    if (!program) {
        printDiagnostics(program.error(), lines);
        return 1;
    }

    std::cout << rv::kUnderline << "\nBINARY CODE:\n" << rv::kNoUnderline << '\n';
    for (std::size_t i = 0; i < program->size(); i++) {
        const uint32_t word = (*program)[i];
        std::cout << i * 4 << "\t" << std::bitset<32>(word) << "\t" << rv::disassemble(*rv::decode(word)) << '\n';
    }

    rv::Cpu cpu(*program, std::cout);
    cpu.run();
    cpu.printRegisters();
    cpu.printMemory();
    return 0;
}
