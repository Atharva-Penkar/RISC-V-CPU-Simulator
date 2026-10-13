// rvsim: runs an RV32IM program on the pipeline (default) or the reference ISS and prints the final state.
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
#include "rv/json.hpp"
#include "rv/timeline.hpp"

namespace {

constexpr std::string_view kUsage =
    "usage: rvsim [--model pipeline|iss] [--trace | --json] [--no-stats] [--max-cycles N]\n"
    "             [--mul-latency N] [--div-latency N] [--load-latency N] [--store-latency N] [input.s | input.hex]\n"
    "  reads assembly from standard input when no input file is given\n"
    "  .hex input holds one hexadecimal instruction word per line (rvasm output)\n"
    "  --no-stats   print only the final registers and memory\n"
    "  --mul-latency, --div-latency, --load-latency, --store-latency N\n"
    "               cycles a multiply or divide spends in EX, or a load or store in MEM (1 to 100,\n"
    "               default 1); the busy stage holds the stages behind it. Pipeline model only\n"
    "  --json       print the whole pipeline run as one JSON document (docs/timeline-format.md),\n"
    "               including assembly errors; the exit status is the same as without it\n"
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
        << "\n  stalls        " << plural(stats.stalls, "cycle") << " (load-use)";
    if (stats.executeStalls != 0)
        out << ", " << plural(stats.executeStalls, "cycle") << " (EX busy)";
    if (stats.memoryStalls != 0)
        out << ", " << plural(stats.memoryStalls, "cycle") << " (MEM busy)";
    out
        << "\n  flushes       " << plural(stats.flushes, "instruction") << " (" << stats.redirects
        << (stats.redirects == 1 ? " taken branch/jump" : " taken branches/jumps") << ")\n";
}

/*0 finished or halted, 2 cycle limit, 3 fault*/
int exitStatus(const rv::RunResult& result, std::size_t maxCycles, bool explain) {
    switch (result.outcome) {
        case rv::Outcome::Finished:
        case rv::Outcome::Halted:
            return 0;
        case rv::Outcome::CycleLimit:
            if (explain)
                std::cerr << "rvsim: stopped after " << maxCycles << " cycles (possible infinite loop)\n";
            return 2;
        case rv::Outcome::Faulted:
            if (explain)
                std::cerr << "rvsim: " << rv::describe(*result.fault) << '\n';
            return 3;
    }
    return 3;
}

template <class Model>
int simulate(Model model, bool trace, bool stats, std::size_t maxCycles) {
    rv::Tracer quiet;
    rv::TextTracer text(std::cout);
    const rv::RunResult result = model.run(trace ? static_cast<rv::Tracer&>(text) : quiet, maxCycles);
    rv::dumpState(model.registers(), model.memory(), std::cout);
    if (stats)
        printStats(result, std::cout);
    std::cout.flush();
    return exitStatus(result, maxCycles, true);
}

}  // namespace

int main(int argc, char** argv) {
    std::string inputPath;
    std::string_view model = "pipeline";
    bool trace = false;
    bool stats = true;
    bool json = false;
    std::size_t maxCycles = rv::kMaxCycles;
    rv::Latencies latencies;
    bool latencyGiven = false;
    auto parseCount = [](std::string_view value, auto& out) {
        auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
        return ec == std::errc{} && end == value.data() + value.size();
    };
    for (int i = 1; i < argc; i++) {
        const std::string_view arg = argv[i];
        unsigned* latency = arg == "--mul-latency"     ? &latencies.multiply
                          : arg == "--div-latency"     ? &latencies.divide
                          : arg == "--load-latency"    ? &latencies.load
                          : arg == "--store-latency"   ? &latencies.store
                                                       : nullptr;
        if (latency && i + 1 < argc) {
            latencyGiven = true;
            if (!parseCount(argv[++i], *latency)) {
                std::cerr << kUsage;
                return 1;
            }
        }
        else if (arg == "--trace")
            trace = true;
        else if (arg == "--no-stats")
            stats = false;
        else if (arg == "--json")
            json = true;
        else if (arg == "--model" && i + 1 < argc)
            model = argv[++i];
        else if (arg == "--max-cycles" && i + 1 < argc) {
            if (!parseCount(argv[++i], maxCycles)) {
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
    if ((model != "pipeline" && model != "iss") || (json && (model != "pipeline" || trace)) || !latencies.valid()
        || (latencyGiven && model != "pipeline")) {
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

    rv::AssembledProgram assembled;
    if (inputPath.ends_with(".hex")) {
        auto words = readHex(source, fileName);
        if (!words)
            return 1;
        assembled.words = std::move(*words);
    }
    else {
        auto result = rv::assembleWithSourceMap(source);
        if (!result) {
            if (json)
                std::cout << rv::diagnosticsToJson("rv-timeline", result.error()) << '\n';
            else
                for (const rv::Diagnostic& diagnostic : result.error())
                    std::cerr << rv::formatDiagnostic(diagnostic, source, fileName);
            return 1;
        }
        assembled = std::move(*result);
    }

    if (json) {
        const rv::Timeline timeline = rv::recordPipeline(std::move(assembled), maxCycles, latencies);
        std::cout << rv::toJson(timeline) << '\n';
        return exitStatus(timeline.result(), maxCycles, false);
    }
    const rv::InstructionMemory program(assembled.words);
    return model == "iss" ? simulate(rv::Iss(program), trace, stats, maxCycles)
                          : simulate(rv::Pipeline(program, latencies), trace, stats, maxCycles);
}
