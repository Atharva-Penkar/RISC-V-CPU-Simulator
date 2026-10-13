// rvasm: assembles RV32I source into machine code, one hexadecimal word per line.
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>

#include "rv/assembler.hpp"
#include "rv/decoder.hpp"
#include "rv/json.hpp"

namespace {

constexpr std::string_view kUsage =
    "usage: rvasm [--listing | --json] [-o output] [input.s]\n"
    "  reads standard input when no input file is given\n"
    "  --listing   print address, encoding and disassembly instead of plain hex\n"
    "  --json      print the program with its source map, or the errors, as JSON\n";

}  // namespace

int main(int argc, char** argv) {
    std::string inputPath;
    std::string outputPath;
    bool listing = false;
    bool json = false;
    for (int i = 1; i < argc; i++) {
        const std::string_view arg = argv[i];
        if (arg == "--listing")
            listing = true;
        else if (arg == "--json")
            json = true;
        else if (arg == "-o" && i + 1 < argc)
            outputPath = argv[++i];
        else if (arg == "-h" || arg == "--help") {
            std::cout << kUsage;
            return 0;
        }
        else if (!arg.empty() && arg[0] != '-' && inputPath.empty())
            inputPath = arg;
        else {
            std::cerr << kUsage;
            return 1;
        }
    }

    std::ifstream file;
    if (!inputPath.empty()) {
        file.open(inputPath);
        if (!file) {
            std::cerr << "rvasm: cannot open " << inputPath << '\n';
            return 1;
        }
    }
    std::istream& in = inputPath.empty() ? std::cin : file;
    const std::string source{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};

    if (json) {
        const auto assembled = rv::assembleWithSourceMap(source);
        std::cout << (assembled ? rv::toJson(*assembled) : rv::diagnosticsToJson("rv-program", assembled.error())) << '\n';
        return assembled ? 0 : 1;
    }

    const auto program = rv::assemble(source);
    if (!program) {
        for (const rv::Diagnostic& diagnostic : program.error())
            std::cerr << rv::formatDiagnostic(diagnostic, source, inputPath.empty() ? "<stdin>" : inputPath);
        std::cerr << program.error().size() << (program.error().size() == 1 ? " error\n" : " errors\n");
        return 1;
    }

    std::ostringstream out;
    for (std::size_t i = 0; i < program->size(); i++) {
        const uint32_t word = (*program)[i];
        if (listing)
            out << rv::toHex(static_cast<uint32_t>(i * 4), 4) << "  " << rv::toHex(word) << "  "
                << rv::disassemble(*rv::decode(word)) << '\n';
        else
            out << rv::toHex(word).substr(2) << '\n';
    }
    if (outputPath.empty()) {
        std::cout << out.str();
        return 0;
    }
    std::ofstream output(outputPath);
    output << out.str();
    if (!output) {
        std::cerr << "rvasm: cannot write " << outputPath << '\n';
        return 1;
    }
    return 0;
}
