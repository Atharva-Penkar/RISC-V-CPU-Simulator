#include "internal.hpp"

namespace rv::detail {

namespace {

bool isSeparator(char c) { return c == ' ' || c == '\t' || c == ',' || c == '\r'; }

}  // namespace

/*statements end at ';' or at the end of a line; '#' starts a comment;
"LABEL:" may stand alone or precede an instruction*/
std::vector<Statement> lex(std::string_view source) {
    std::vector<Statement> statements;
    std::size_t lineNumber = 0;
    while (!source.empty() || lineNumber == 0) {
        const std::size_t newline = source.find('\n');
        std::string_view line = source.substr(0, newline);
        source.remove_prefix(newline == std::string_view::npos ? source.size() : newline + 1);
        lineNumber++;
        if (const std::size_t comment = line.find('#'); comment != std::string_view::npos)
            line = line.substr(0, comment);

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
            Token token{line.substr(start, i - start), lineNumber, start + 1};
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

}  // namespace rv::detail
