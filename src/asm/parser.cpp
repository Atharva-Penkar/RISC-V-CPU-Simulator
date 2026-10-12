#include <algorithm>
#include <cctype>
#include <charconv>
#include <span>
#include <system_error>
#include <tuple>

#include "internal.hpp"

namespace rv {

namespace detail {

std::string toUpper(std::string_view text) {
    std::string upper(text);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return upper;
}

namespace {

bool fitsSigned(int64_t value, unsigned width) {
    return value >= -(int64_t{1} << (width - 1)) && value < (int64_t{1} << (width - 1));
}

Result<Reg, Diagnostic> parseRegister(const Token& token) {
    const std::string_view text = token.text;
    unsigned index = 0;
    if (text.size() >= 2 && text.size() <= 3 && (text[0] == 'x' || text[0] == 'X')) {
        const char* end = text.data() + text.size();
        auto [last, ec] = std::from_chars(text.data() + 1, end, index);
        if (ec == std::errc{} && last == end)
            if (auto reg = Reg::fromIndex(index))
                return *reg;
    }
    return fail(diagnose(token, concat("invalid register '", text, "' (registers are x0 to x31)")));
}

/*hexadecimal 0x... (up to 8 digits, 32-bit two's complement, so 0xFFFFFFFE = -2) or signed decimal*/
Result<int64_t, Diagnostic> parseImmediate(const Token& token) {
    std::string_view text = token.text;
    bool negative = false;
    if (!text.empty() && (text[0] == '-' || text[0] == '+')) {
        negative = text[0] == '-';
        text.remove_prefix(1);
    }
    const bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    if (hex)
        text.remove_prefix(2);
    uint64_t magnitude = 0;
    const char* end = text.data() + text.size();
    auto [last, ec] = std::from_chars(text.data(), end, magnitude, hex ? 16 : 10);
    if (text.empty() || ec != std::errc{} || last != end || (hex && text.size() > 8) || magnitude > 0xFFFFFFFFull)
        return fail(diagnose(token, concat("invalid immediate '", token.text, "'")));
    const int64_t value = hex ? static_cast<int32_t>(static_cast<uint32_t>(magnitude)) : static_cast<int64_t>(magnitude);
    return negative ? -value : value;
}

/*an immediate that must fit a field of the given width, as a signed or an unsigned bit pattern*/
Result<int32_t, Diagnostic> parseField(const Token& token, unsigned width) {
    auto value = parseImmediate(token);
    if (!value)
        return fail(value.error());
    if (*value < -(int64_t{1} << (width - 1)) || *value >= (int64_t{1} << width))
        return fail(diagnose(token, concat("immediate '", token.text, "' does not fit in ", std::to_string(width), " bits")));
    return signExtend(static_cast<uint32_t>(*value), width);
}

/*PC-relative byte offset to a label, or a numeric byte offset*/
Result<int32_t, Diagnostic> parseOffset(const Token& token, uint32_t index, unsigned width, const LabelTable& labels) {
    int64_t offset = 0;
    if (auto label = labels.find(toUpper(token.text)); label != labels.end())
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
    if (!fitsSigned(offset, width))
        return fail(diagnose(token, concat("target '", token.text, "' is out of range")));
    return static_cast<int32_t>(offset);
}

/*imm[xN] or imm(xN); an empty immediate means 0*/
Result<std::pair<int32_t, Reg>, Diagnostic> parseMemory(const Token& token) {
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

std::size_t operandCount(Kind kind) {
    switch (kind) {
        case Kind::Load:
        case Kind::Store:
        case Kind::Lui:
        case Kind::Auipc:
        case Kind::Jal: return 2;
        case Kind::System: return 0;
        default: return 3;
    }
}

std::optional<Diagnostic> defineLabel(const Token& label, uint32_t index, LabelTable& labels) {
    const std::string_view name = label.text;
    auto validChar = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.'; };
    if (name.empty() || std::isdigit(static_cast<unsigned char>(name[0])) || !std::all_of(name.begin(), name.end(), validChar))
        return diagnose(label, concat("invalid label name '", name, "'"));
    if (!labels.emplace(toUpper(name), index).second)
        return diagnose(label, concat("label '", name, "' is defined more than once"));
    return std::nullopt;
}

}  // namespace

Result<DecodedInst, Diagnostic> parseInstruction(const Statement& statement, uint32_t index, const LabelTable& labels) {
    const Token& mnemonic = statement.words[0];
    const InstrSpec* spec = findSpec(toUpper(mnemonic.text));
    if (!spec)
        return fail(diagnose(mnemonic, concat("unknown instruction '", mnemonic.text, "'")));
    const std::span<const Token> operands = std::span(statement.words).subspan(1);
    const std::size_t expected = operandCount(spec->kind);
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
            if (!error) error = immediate(parseOffset(operands[2], index, 13, labels));
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
            if (!error) error = immediate(parseOffset(operands[1], index, 21, labels));
            break;
        case Kind::Jalr:
            /*jalr rd,rs1,imm|label  -  a label gives its absolute byte address*/
            error = reg(0, inst.rd);
            if (!error) error = reg(1, inst.rs1);
            if (error)
                break;
            if (auto label = labels.find(toUpper(operands[2].text)); label != labels.end()) {
                const int64_t address = int64_t{label->second} * 4;
                if (!fitsSigned(address, 12))
                    error = diagnose(operands[2], concat("label '", operands[2].text, "' is out of range for JALR"));
                else
                    inst.imm = static_cast<int32_t>(address);
            }
            else
                error = immediate(parseField(operands[2], 12));
            break;
        case Kind::System:
            break;
    }
    if (error)
        return fail(*error);
    return inst;
}

namespace {

/*pass 1 assigns label addresses, pass 2 parses and encodes every instruction*/
Result<Program, std::vector<Diagnostic>> assembleStatements(const std::vector<Statement>& statements) {
    std::vector<Diagnostic> diagnostics;
    LabelTable labels;
    uint32_t index = 0;
    for (const Statement& statement : statements) {
        if (statement.label)
            if (auto error = defineLabel(*statement.label, index, labels))
                diagnostics.push_back(*error);
        if (!statement.words.empty())
            index++;
    }

    Program program;
    for (const Statement& statement : statements) {
        if (statement.words.empty())
            continue;
        auto inst = parseInstruction(statement, static_cast<uint32_t>(program.size()), labels);
        if (inst)
            program.push_back(encode(*inst));
        else {
            diagnostics.push_back(inst.error());
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

}  // namespace

}  // namespace detail

Result<Program, std::vector<Diagnostic>> assemble(std::string_view source) {
    return detail::assembleStatements(detail::lex(source));
}

std::string formatDiagnostic(const Diagnostic& diagnostic, std::string_view source, std::string_view fileName) {
    std::string_view line;
    for (std::size_t number = 1; number <= diagnostic.line; number++) {
        const std::size_t newline = source.find('\n');
        line = source.substr(0, newline);
        source.remove_prefix(newline == std::string_view::npos ? source.size() : newline + 1);
    }
    if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);
    return detail::concat(fileName, ":", std::to_string(diagnostic.line), ":", std::to_string(diagnostic.column),
                          ": error: ", diagnostic.message, "\n    ", line, "\n    ",
                          std::string(diagnostic.column - 1, ' '), "^\n");
}

}  // namespace rv
