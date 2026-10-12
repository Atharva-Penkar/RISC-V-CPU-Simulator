// rvsim: runs an RV32I program on the pipeline (default) or the reference ISS and prints the final state.
#include <charconv>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "rv/assembler.hpp"
#include "rv/cpu.hpp"

namespace {

constexpr std::string_view kUsage =
    "usage: rvsim [--model pipeline|iss] [--trace] [--max-cycles N] [input.s | input.hex]\n"
    "  reads assembly from standard input when no input file is given\n"
    "  .hex input holds one hexadecimal instruction word per line (rvasm output)\n"
    "  exit status: 0 finished, 1 bad input, 2 stopped by the cycle limit\n";

std::optional<rv::Program> readHex(std::string_view text, std::string_view fileName) {
    rv::Program program;
    std::size_t lineNumber = 0;
    while (!text.empty()) {
        const std::size_t newline = text.find('\n');
        std::string_view line = text.substr(0, newline);
        text.remove_prefix(newline == std::string_view::npos ? text.size() : newline + 1);
        lineNumber++;
        line = line.substr(0, line.find('#'));
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.remove_suffix(1);
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            line.remove_prefix(1);
        if (line.empty())
            continue;
        uint32_t word = 0;
        auto [end, ec] = std::from_chars(line.data(), line.data() + line.size(), word, 16);
        if (ec != std::errc{} || end != line.data() + line.size()) {
            std::cerr << fileName << ':' << lineNumber << ": error: invalid instruction word '" << line << "'\n";
            return std::nullopt;
        }
        program.push_back(word);
    }
    return program;
}

template <class Model>
int simulate(const rv::Program& program, bool trace, std::size_t maxCycles) {
    Model model{rv::InstructionMemory(program)};
    rv::TextTracer tracer(trace ? std::cout : std::cerr, trace);
    const rv::RunResult result = model.run(tracer, maxCycles);
    rv::dumpState(model.registers(), model.memory(), std::cout);
    std::cout.flush();
    std::cerr << result.retired << " instructions retired in " << result.cycles << " cycles\n";
    if (!result.completed) {
        std::cerr << "rvsim: stopped after " << maxCycles << " cycles (possible infinite loop)\n";
        return 2;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string inputPath;
    std::string_view model = "pipeline";
    bool trace = false;
    std::size_t maxCycles = rv::kMaxCycles;
    for (int i = 1; i < argc; i++) {
        const std::string_view arg = argv[i];
        if (arg == "--trace")
            trace = true;
        else if (arg == "--model" && i + 1 < argc)
            model = argv[++i];
        else if (arg == "--max-cycles" && i + 1 < argc) {
            const std::string_view value = argv[++i];
            auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), maxCycles);
            if (ec != std::errc{} || end != value.data() + value.size()) {
                std::cerr << kUsage;
                return 1;
            }
        }
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
    if (model != "pipeline" && model != "iss") {
        std::cerr << kUsage;
        return 1;
    }

    std::ifstream file;
    if (!inputPath.empty()) {
        file.open(inputPath);
        if (!file) {
            std::cerr << "rvsim: cannot open " << inputPath << '\n';
            return 1;
        }
    }
    std::istream& in = inputPath.empty() ? std::cin : file;
    const std::string source{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const std::string_view fileName = inputPath.empty() ? std::string_view("<stdin>") : std::string_view(inputPath);

    rv::Program program;
    if (inputPath.ends_with(".hex")) {
        auto words = readHex(source, fileName);
        if (!words)
            return 1;
        program = std::move(*words);
    }
    else {
        auto assembled = rv::assemble(source);
        if (!assembled) {
            for (const rv::Diagnostic& diagnostic : assembled.error())
                std::cerr << rv::formatDiagnostic(diagnostic, source, fileName);
            return 1;
        }
        program = std::move(*assembled);
    }

    return model == "iss" ? simulate<rv::Iss>(program, trace, maxCycles)
                          : simulate<rv::Pipeline>(program, trace, maxCycles);
}
