#include "check.hpp"

#include <algorithm>
#include <string>

#include "rv/assembler.hpp"
#include "rv/json.hpp"
#include "rv/timeline.hpp"

namespace test {

namespace {

rv::Timeline recordOrFail(std::string_view source, std::source_location where = std::source_location::current()) {
    auto assembled = rv::assembleWithSourceMap(source);
    if (!assembled) {
        fail("assembly failed: " + assembled.error().front().message, where);
        return rv::recordPipeline({});
    }
    return rv::recordPipeline(std::move(*assembled));
}

const rv::StageSlot& slot(const rv::Timeline& t, std::size_t cycle, rv::Stage stage) {
    return t.at(cycle).stages[static_cast<std::size_t>(stage)];
}

bool holds(const rv::Timeline& t, std::size_t cycle, rv::Stage stage, uint32_t instance) {
    const rv::StageSlot& s = slot(t, cycle, stage);
    return s.kind == rv::StageSlot::Kind::Instruction && s.instance == instance;
}

bool isBubble(const rv::Timeline& t, std::size_t cycle, rv::Stage stage, rv::BubbleReason reason) {
    const rv::StageSlot& s = slot(t, cycle, stage);
    return s.kind == rv::StageSlot::Kind::Bubble && s.bubble == reason;
}

}  // namespace

void addTimelineTests(Suite& suite) {
    using rv::Stage;

    suite.add("assembler source map", [] {
        const auto assembled = rv::assembleWithSourceMap("addi x1,x0,1\n  loop: add x2,x1,x1 # c\n\nsw x2, 0[x0]; ebreak");
        expect(assembled.has_value());
        if (!assembled)
            return;
        expectEq(assembled->sources.size(), std::size_t{4});
        const std::vector<std::pair<std::size_t, std::size_t>> expected = {{1, 1}, {2, 9}, {4, 1}, {4, 15}};
        for (std::size_t i = 0; i < expected.size() && i < assembled->sources.size(); i++) {
            expectEq(assembled->sources[i].line, expected[i].first);
            expectEq(assembled->sources[i].column, expected[i].second);
        }
    });

    suite.add("stage occupancy, forwarding and register writes", [] {
        const rv::Timeline t = recordOrFail("addi x1,x0,5; addi x2,x1,1");
        expectEq(t.cycleCount(), std::size_t{6});
        expect(holds(t, 1, Stage::IF, 0) && slot(t, 1, Stage::ID).kind == rv::StageSlot::Kind::Empty);
        expect(holds(t, 2, Stage::IF, 1) && holds(t, 2, Stage::ID, 0));
        expect(holds(t, 3, Stage::ID, 1) && holds(t, 3, Stage::EX, 0));
        expect(holds(t, 5, Stage::WB, 0) && holds(t, 6, Stage::WB, 1));
        expect(t.at(1).nextPcSource == rv::NextPcSource::Sequential && t.at(1).nextPc == 4);

        const auto& forwards = t.at(4).forwards;   // addi x2 in EX takes x1 from EX/MEM
        expectEq(forwards.size(), std::size_t{1});
        if (!forwards.empty()) {
            expect(forwards[0].operand == rv::Operand::Rs1 && forwards[0].from == rv::ForwardSource::ExMem);
            expectEq(forwards[0].value, 5u);
            expectEq(forwards[0].producer, 0u);
            expectEq(forwards[0].consumer, 1u);
        }
        const auto& exec = t.at(4).execute;
        expect(exec.has_value() && exec->a == 5 && exec->b == 1 && exec->result == 6 && exec->bFromImmediate);

        const auto& write = t.at(5).registerWrite;
        expect(write && write->reg.index() == 1 && write->value == 5 && write->previous == 0
               && write->source == rv::WriteSource::Alu && write->instance == 0);
        for (const rv::InstanceRecord& instance : t.instances())
            expect(instance.fate == rv::Fate::Retired, "every instance retires");
    });

    suite.add("load-use stall record and bubble", [] {
        const rv::Timeline t = recordOrFail("lw x1,0[x0]; add x2,x1,x1");
        const auto& stall = t.at(3).stall;
        expect(stall && stall->waiting == 1 && stall->load == 0 && stall->reg.index() == 1, "stall record");
        expect(slot(t, 3, Stage::ID).stalled, "ID marked stalled");
        expect(t.at(3).nextPcSource == rv::NextPcSource::Hold, "a stall holds the PC");
        expect(isBubble(t, 4, Stage::EX, rv::BubbleReason::Stall), "bubble in EX");
        expect(holds(t, 4, Stage::ID, 1), "waiting instruction still in ID");
        const auto& forwards = t.at(5).forwards;
        expectEq(forwards.size(), std::size_t{2});
        for (const auto& f : forwards)
            expect(f.from == rv::ForwardSource::MemWb && f.producer == 0 && f.consumer == 1);
        const auto& load = t.at(4).memory;
        expect(load && !load->store && load->address == 0 && load->width == 4);
    });

    suite.add("a stall holds the next fetch in IF", [] {
        const rv::Timeline t = recordOrFail("lw x1,0[x0]; add x2,x1,x1; addi x3,x0,1");
        const rv::StageSlot& fetch = slot(t, 3, Stage::IF);
        expect(fetch.kind == rv::StageSlot::Kind::Instruction && fetch.stalled, "IF stalled");
        expect(holds(t, 4, Stage::IF, fetch.instance), "same instance fetched next cycle");
        expect(t.at(3).nextPcSource == rv::NextPcSource::Hold && t.at(3).nextPc == 8);
    });

    suite.add("flushes name the branch and the squashed instances; loops create new instances", [] {
        const rv::Timeline t = recordOrFail(
            "addi x1,x0,2\nloop: addi x1,x1,-1\nbne x1,x0,loop\naddi x3,x0,9\naddi x4,x0,9");
        std::size_t flushes = 0;
        for (const rv::CycleRecord& c : t.cycles()) {
            if (!c.flush)
                continue;
            flushes++;
            const rv::InstanceRecord& by = t.instances().at(c.flush->by);
            expectEq(by.instruction.value_or(99), 2u);    // the bne
            expectEq(c.flush->squashed.size(), std::size_t{2});
            for (const uint32_t squashed : c.flush->squashed)
                expect(t.instances().at(squashed).fate == rv::Fate::Flushed);
            expect(c.nextPcSource == rv::NextPcSource::BranchTarget && c.nextPc == 4);
            expect(isBubble(t, c.cycle + 1, Stage::ID, rv::BubbleReason::Flush));
            expect(isBubble(t, c.cycle + 1, Stage::EX, rv::BubbleReason::Flush));
        }
        expectEq(flushes, std::size_t{1});

        std::vector<uint32_t> occurrences;
        for (const rv::InstanceRecord& instance : t.instances())
            if (instance.instruction == 1u)
                occurrences.push_back(instance.occurrence);
        expect(occurrences == std::vector<uint32_t>{1, 2}, "loop body fetched twice");

        bool sameLineTwice = false;     // two instances of the loop body in different stages
        for (const rv::CycleRecord& c : t.cycles()) {
            std::size_t count = 0;
            for (const rv::StageSlot& s : c.stages)
                count += s.kind == rv::StageSlot::Kind::Instruction && t.instances().at(s.instance).instruction == 1u;
            sameLineTwice = sameLineTwice || count == 2;
        }
        expect(sameLineTwice, "the same line occupies two stages in some cycle");
        expectEq(t.finalRegisters().read(rv::Reg::fromField(3)), 9u);
    });

    suite.add("halt and fault outcomes", [] {
        const rv::Timeline halted = recordOrFail("addi x1,x0,1; ebreak; addi x2,x0,2; addi x3,x0,3");
        expect(halted.result().outcome == rv::Outcome::Halted);
        expect(halted.stopInstance() == 1u);
        expect(halted.instances().at(1).fate == rv::Fate::Halted);
        expect(halted.instances().at(2).fate == rv::Fate::Discarded && halted.instances().at(3).fate == rv::Fate::Discarded);

        const rv::Timeline faulted = recordOrFail("addi x1,x0,256; lw x2,0[x1]; addi x3,x0,1");
        expect(faulted.result().outcome == rv::Outcome::Faulted && faulted.result().fault);
        expect(faulted.stopInstance() == 1u);
        expect(faulted.instances().at(1).fate == rv::Fate::Faulted);

        const rv::Timeline limited = rv::recordPipeline(*rv::assembleWithSourceMap("l: jal x0,l"), 20);
        expect(limited.result().outcome == rv::Outcome::CycleLimit);
        expectEq(limited.cycleCount(), std::size_t{20});
    });

    suite.add("replayed state matches the final state", [] {
        const rv::Timeline t = recordOrFail(
            "addi x1,x0,8; addi x4,x0,-2; sw x4,0[x1]; sh x1,6[x1]; lw x5,0[x1]; sb x5,1[x0]");
        const auto registers = t.registersAfter(t.cycleCount());
        for (unsigned r = 0; r < 32; r++)
            expectEq(registers[r], t.finalRegisters().read(rv::Reg::fromField(r)));
        const rv::DataMemory memory = t.memoryAfter(t.cycleCount());
        for (uint32_t address = 0; address < memory.size(); address += 4)
            expectEq(memory.read(address, 4).value_or(1), t.finalMemory().read(address, 4).value_or(2));
        expectEq(t.registersAfter(0)[1], 0u);
        const auto store = std::find_if(t.cycles().begin(), t.cycles().end(),
                                        [](const rv::CycleRecord& c) { return c.memory && c.memory->store; });
        expect(store != t.cycles().end() && store->memory->value == 0xFFFFFFFEu && store->memory->previous == 0);
    });

    suite.add("JSON documents", [] {
        const std::string timeline = rv::toJson(recordOrFail("addi x1,x0,1"));
        expect(timeline.front() == '{' && timeline.back() == '}');
        expect(timeline.find("\"format\":\"rv-timeline\"") != std::string::npos);
        expect(timeline.find("\"source\":{\"line\":1,\"column\":1}") != std::string::npos);
        const std::string errors = rv::diagnosticsToJson("rv-timeline", {{1, 2, "bad \"quote\"\n"}});
        expect(errors.find("\"message\":\"bad \\\"quote\\\"\\n\"") != std::string::npos, "escaping");
        expect(errors.find("\"ok\":false") != std::string::npos);
    });
}

}  // namespace test
