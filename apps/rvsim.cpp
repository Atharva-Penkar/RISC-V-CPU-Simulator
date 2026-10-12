// rvsim: runs an RV32I program on the pipeline (default) or the reference ISS and prints the final state.
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include "rv/assembler.hpp"
#include "rv/cpu.hpp"

namespace {

constexpr std::string_view kUsage =
    "usage: rvsim [--model pipeline|iss] [--trace] [--no-stats] [--max-cycles N] [input.s | input.hex]\n"
    "  reads assembly from standard input when no input file is given\n"
    "  .hex input holds one hexadecimal instruction word per line (rvasm output)\n"
    "  --no-stats   print only the final registers and memory\n"
    "  exit status: 0 finished or halted, 1 bad input, 2 cycle limit reached, 3 fault\n";

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

std::string plural(std::size_t count, std::string_view noun) {
    return std::to_string(count) + " " + std::string(noun) + (count == 1 ? "" : "s");
}

void printStats(const rv::RunResult& result, std::ostream& out) {
    const rv::RunStats& stats = result.stats;
    out << "statistics\n"
        << "  outcome       " << rv::toString(result.outcome);
    if (result.outcome == rv::Outcome::Halted)
        out << " at pc " << rv::toHex(result.haltPc);
    out << "\n  cycles        " << stats.cycles
        << "\n  instructions  " << stats.retired
        << "\n  CPI           " << std::fixed << std::setprecision(2) << stats.cpi()
        << "\n  stalls        " << plural(stats.stalls, "cycle") << " (load-use)"
        << "\n  flushes       " << plural(stats.flushes, "instruction") << " (" << stats.redirects
        << (stats.redirects == 1 ? " taken branch/jump" : " taken branches/jumps") << ")\n";
}

template <class Model>
int simulate(const rv::Program& program, bool trace, bool stats, std::size_t maxCycles) {
    Model model{rv::InstructionMemory(program)};
    rv::Tracer quiet;
    rv::TextTracer text(std::cout);
    const rv::RunResult result = model.run(trace ? static_cast<rv::Tracer&>(text) : quiet, maxCycles);
    rv::dumpState(model.registers(), model.memory(), std::cout);
    if (stats)
        printStats(result, std::cout);
    std::cout.flush();
    switch (result.outcome) {
        case rv::Outcome::Finished:
        case rv::Outcome::Halted:
            return 0;
        case rv::Outcome::CycleLimit:
            std::cerr << "rvsim: stopped after " << maxCycles << " cycles (possible infinite loop)\n";
            return 2;
        case rv::Outcome::Faulted:
            std::cerr << "rvsim: " << rv::describe(*result.fault) << '\n';
            return 3;
    }
    return 3;
}

}  // namespace

int main(int argc, char** argv) {
    std::string inputPath;
    std::string_view model = "pipeline";
    bool trace = false;
    bool stats = true;
    std::size_t maxCycles = rv::kMaxCycles;
    for (int i = 1; i < argc; i++) {
        const std::string_view arg = argv[i];
        if (arg == "--trace")
            trace = true;
        else if (arg == "--no-stats")
            stats = false;
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

    return model == "iss" ? simulate<rv::Iss>(program, trace, stats, maxCycles)
                          : simulate<rv::Pipeline>(program, trace, stats, maxCycles);
}
