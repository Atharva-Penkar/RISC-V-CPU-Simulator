#include "rv/timeline.hpp"

namespace rv {

Timeline recordPipeline(AssembledProgram program, std::size_t maxCycles, Latencies latencies) {
    Timeline timeline;
    timeline.latencies_ = latencies;
    timeline.words_ = std::move(program.words);
    timeline.sources_ = std::move(program.sources);
    timeline.occurrences_.assign(timeline.words_.size(), 0);

    const InstructionMemory imem(timeline.words_);
    timeline.decoded_.reserve(timeline.words_.size());
    for (uint32_t i = 0; i < timeline.words_.size(); i++)
        timeline.decoded_.push_back(imem.decoded(i * 4));

    Pipeline pipeline(imem, latencies);
    pipeline.record(timeline);
    Tracer quiet;
    timeline.result_ = pipeline.run(quiet, maxCycles);
    timeline.finalRegisters_ = pipeline.registers();
    timeline.finalMemory_ = pipeline.memory();
    return timeline;
}

std::array<uint32_t, 32> Timeline::registersAfter(std::size_t cycle) const {
    std::array<uint32_t, 32> values{};
    for (std::size_t i = 0; i < cycle && i < cycles_.size(); i++)
        if (const auto& write = cycles_[i].registerWrite)
            values[write->reg.index()] = write->value;
    return values;
}

DataMemory Timeline::memoryAfter(std::size_t cycle) const {
    DataMemory memory(finalMemory_.size());
    for (std::size_t i = 0; i < cycle && i < cycles_.size(); i++)
        if (const auto& access = cycles_[i].memory; access && access->store)
            memory.write(access->address, access->width, access->value);
    return memory;
}

}  // namespace rv
