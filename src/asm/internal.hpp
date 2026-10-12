// Assembler internals shared by the lexer, parser and encoder.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "rv/assembler.hpp"

namespace rv::detail {

struct Token {
    std::string_view text;  // view into the assembly source
    std::size_t line;       // 1-based
    std::size_t column;     // 1-based
};

struct Statement {
    std::optional<Token> label;
    std::vector<Token> words;   // mnemonic followed by operands
};

/*splits the source into statements; tokens view into source, which must outlive them*/
std::vector<Statement> lex(std::string_view source);

/*upper-cased label -> instruction index*/
using LabelTable = std::map<std::string, uint32_t, std::less<>>;

/*fields of the instruction in statement (whose words are not empty) at the given index*/
Result<DecodedInst, Diagnostic> parseInstruction(const Statement& statement, uint32_t index, const LabelTable& labels);

inline Diagnostic diagnose(const Token& token, std::string message) {
    return {token.line, token.column, std::move(message)};
}

std::string toUpper(std::string_view text);

template <class... Parts>
std::string concat(const Parts&... parts) {
    std::string text;
    ((text += parts), ...);
    return text;
}

}  // namespace rv::detail
