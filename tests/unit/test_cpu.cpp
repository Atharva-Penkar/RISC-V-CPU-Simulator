#include "check.hpp"

#include <string>
#include <vector>

#include "rv/assembler.hpp"
#include "rv/cpu.hpp"
#include "rv/decoder.hpp"

namespace test {

namespace {

rv::Program assembleOrFail(std::string_view source, std::source_location where = std::source_location::current()) {
    auto program = rv::assemble(source);
    if (!program) {
        fail("assembly failed: " + program.error().front().message, where);
        return {};
    }
    return *program;
}

uint32_t reg(const rv::RegisterFile& regs, unsigned index) { return regs.read(rv::Reg::fromField(index)); }

struct PipelineRun {
    rv::Program program;
    rv::Pipeline pipeline;
    rv::RunResult result;

    explicit PipelineRun(rv::Program code, std::size_t maxCycles = rv::kMaxCycles, rv::Latencies latencies = {})
        : program(std::move(code)), pipeline(rv::InstructionMemory(program), latencies) {
        rv::Tracer quiet;
        result = pipeline.run(quiet, maxCycles);
    }
    uint32_t x(unsigned index) const { return reg(pipeline.registers(), index); }
    uint32_t word(uint32_t address) const { return pipeline.memory().read(address, 4).value_or(0xDEADBEEF); }
};

PipelineRun runPipeline(std::string_view source, std::source_location where = std::source_location::current()) {
    return PipelineRun(assembleOrFail(source, where));
}

PipelineRun runPipeline(std::string_view source, rv::Latencies latencies,
                        std::source_location where = std::source_location::current()) {
    return PipelineRun(assembleOrFail(source, where), rv::kMaxCycles, latencies);
}

/*runs program on both models; the pipeline must end in the same state, with the same outcome, as the ISS*/
void expectModelsAgree(const rv::Program& program, rv::Latencies latencies,
                       std::source_location where = std::source_location::current()) {
    rv::Tracer quiet;
    rv::Iss iss{rv::InstructionMemory(program)};
    rv::Pipeline pipeline{rv::InstructionMemory(program), latencies};
    const rv::RunResult a = iss.run(quiet);
    const rv::RunResult b = pipeline.run(quiet);
    if (a.outcome != b.outcome || a.stats.retired != b.stats.retired)
        fail("models end differently: ISS " + rv::toString(a.outcome) + " after " + std::to_string(a.stats.retired)
                 + " instructions, pipeline " + rv::toString(b.outcome) + " after " + std::to_string(b.stats.retired),
             where);
    if (a.fault.has_value() != b.fault.has_value()
        || (a.fault && (a.fault->kind != b.fault->kind || a.fault->pc != b.fault->pc)))
        fail("models report different faults", where);
    for (unsigned i = 0; i < 32; i++)
        if (reg(iss.registers(), i) != reg(pipeline.registers(), i))
            fail("register x" + std::to_string(i) + " differs between ISS and pipeline", where);
    for (uint32_t address = 0; address < iss.memory().size(); address += 4)
        if (iss.memory().read(address, 4) != pipeline.memory().read(address, 4))
            fail("memory word " + std::to_string(address) + " differs between ISS and pipeline", where);
}

/*with the classic one-cycle stages, and with slow multiply, divide and memory: timing must never change results*/
void expectModelsAgree(const rv::Program& program, std::source_location where = std::source_location::current()) {
    for (const rv::Latencies latencies : {rv::Latencies{}, rv::Latencies{3, 1, 1, 1}, rv::Latencies{1, 1, 2, 3},
                                          rv::Latencies{4, 9, 3, 2}})
        expectModelsAgree(program, latencies, where);
}

void expectModelsAgree(std::string_view source, std::source_location where = std::source_location::current()) {
    expectModelsAgree(assembleOrFail(source, where), where);
}

}  // namespace

void addCpuTests(Suite& suite) {
    suite.add("hazard unit forwarding priority", [] {
        const rv::Reg x0 = rv::Reg::fromField(0);
        const rv::Reg x1 = rv::Reg::fromField(1);
        const rv::Reg x2 = rv::Reg::fromField(2);
        using rv::ForwardSource;
        expect(rv::HazardUnit::forward(x1, {true, x1}, {true, x1}) == ForwardSource::ExMem, "newest value wins");
        expect(rv::HazardUnit::forward(x1, {true, x2}, {true, x1}) == ForwardSource::MemWb);
        expect(rv::HazardUnit::forward(x1, {false, x1}, {false, x1}) == ForwardSource::RegisterFile);
        expect(rv::HazardUnit::forward(x0, {true, x0}, {true, x0}) == ForwardSource::RegisterFile, "x0 is never forwarded");
    });

    suite.add("hazard unit load-use detection", [] {
        const auto add = *rv::decode(0x001080B3u);     // add x1, x1, x1
        const auto addi = *rv::decode(0x00100093u);    // addi x1, x0, 1 (reads only x0)
        const rv::PendingWrite loadX1{true, rv::Reg::fromField(1)};
        expect(rv::HazardUnit::loadUseHazard(add, rv::controlFor(add.kind()), loadX1));
        expect(!rv::HazardUnit::loadUseHazard(addi, rv::controlFor(addi.kind()), loadX1));
        expect(!rv::HazardUnit::loadUseHazard(add, rv::controlFor(add.kind()), {false, rv::Reg::fromField(1)}));
    });

    suite.add("forwarding removes stalls between ALU instructions", [] {
        const PipelineRun run = runPipeline("addi x1,x0,5; addi x2,x0,7; add x3,x1,x2; sub x4,x3,x1");
        expectEq(run.x(3), 12u);    // x1 from MEM/WB, x2 from EX/MEM
        expectEq(run.x(4), 7u);
        expectEq(run.result.stats.stalls, std::size_t{0});
        expectEq(run.result.stats.cycles, std::size_t{8});     // 4 instructions + 4 to fill the pipeline
    });

    suite.add("store data and jump targets are forwarded", [] {
        const PipelineRun store = runPipeline("addi x1,x0,9; sw x1,4[x0]");
        expectEq(store.word(4), 9u);
        const PipelineRun jump = runPipeline("addi x1,x0,12; jalr x5,x1,0; addi x2,x0,1; addi x3,x0,2");
        expectEq(jump.x(2), 0u);
        expectEq(jump.x(3), 2u);
        expectEq(jump.x(5), 8u);
    });

    suite.add("load-use costs exactly one stall cycle", [] {
        const PipelineRun dependent = runPipeline("lw x1,0[x0]; add x2,x1,x1");
        expectEq(dependent.result.stats.stalls, std::size_t{1});
        expectEq(dependent.result.stats.cycles, std::size_t{7});
        const PipelineRun separated = runPipeline("addi x5,x0,21; sw x5,0[x0]; lw x1,0[x0]; addi x3,x0,1; add x2,x1,x1");
        expectEq(separated.result.stats.stalls, std::size_t{0});
        expectEq(separated.x(2), 42u);
        const PipelineRun loadStore = runPipeline("addi x5,x0,3; sw x5,0[x0]; lw x1,0[x0]; sw x1,8[x0]");
        expectEq(loadStore.result.stats.stalls, std::size_t{1});
        expectEq(loadStore.word(8), 3u);
    });

    suite.add("taken branch flushes IF/ID and ID/EX", [] {
        const PipelineRun run = runPipeline("beq x0,x0,skip; addi x1,x0,1; addi x3,x0,3; skip: addi x2,x0,2");
        expectEq(run.x(1), 0u);
        expectEq(run.x(3), 0u);
        expectEq(run.x(2), 2u);
        expectEq(run.result.stats.flushes, std::size_t{2});
        expectEq(run.result.stats.redirects, std::size_t{1});
        expectEq(run.result.stats.retired, std::size_t{2});
        expectEq(run.result.stats.cycles, std::size_t{8});     // two-cycle branch penalty
        const PipelineRun notTaken = runPipeline("bne x0,x0,skip; addi x1,x0,1; skip: addi x2,x0,2");
        expectEq(notTaken.result.stats.flushes, std::size_t{0});
        expectEq(notTaken.x(1), 1u);
    });

    suite.add("a multi-cycle multiply or divide holds EX and the stages behind it", [] {
        const char* program = "addi x1,x0,6; addi x2,x0,7; mul x3,x1,x2; add x4,x3,x3";
        const PipelineRun fast = runPipeline(program);
        const PipelineRun slow = runPipeline(program, {4, 1, 1, 1});
        expectEq(slow.x(4), 84u);
        expectEq(slow.result.stats.cycles, fast.result.stats.cycles + 3);
        expectEq(slow.result.stats.executeStalls, std::size_t{3});
        expectEq(slow.result.stats.stalls, std::size_t{0});     // the dependent add is forwarded, not stalled
        const PipelineRun divide = runPipeline("addi x1,x0,85; addi x2,x0,6; div x3,x1,x2; rem x4,x1,x2", {1, 10, 1, 1});
        expectEq(divide.x(3), 14u);
        expectEq(divide.x(4), 1u);
        expectEq(divide.result.stats.executeStalls, std::size_t{18});
    });

    suite.add("a multi-cycle load or store holds MEM and the stages behind it", [] {
        const char* program = "addi x1,x0,5; sw x1,0[x0]; lw x2,0[x0]; add x3,x2,x2";
        const PipelineRun fast = runPipeline(program);
        const PipelineRun slow = runPipeline(program, {1, 1, 3, 2});
        expectEq(slow.x(3), 10u);
        expectEq(slow.result.stats.memoryStalls, std::size_t{3});     // 2 for the load, 1 for the store
        expectEq(slow.result.stats.stalls, std::size_t{1});     // the load-use stall still happens
        expectEq(slow.result.stats.cycles, fast.result.stats.cycles + 3);
    });

    suite.add("CPI", [] {
        const PipelineRun run = runPipeline("addi x1,x0,1; addi x2,x0,2; addi x3,x0,3; addi x4,x0,4");
        expect(run.result.stats.cpi() == 2.0, "8 cycles / 4 instructions");
        rv::RunStats empty;
        expect(empty.cpi() == 0.0);
    });

    suite.add("ecall and ebreak halt after older instructions complete", [] {
        const PipelineRun run = runPipeline("addi x1,x0,3; sw x1,0[x0]; ebreak; sw x1,4[x0]; addi x2,x0,1");
        expect(run.result.outcome == rv::Outcome::Halted, "halted");
        expectEq(run.result.haltPc, 8u);
        expectEq(run.word(0), 3u);
        expectEq(run.word(4), 0u);
        expectEq(run.x(2), 0u);
        expectEq(run.result.stats.retired, std::size_t{3});
        expect(runPipeline("ecall").result.outcome == rv::Outcome::Halted, "ecall");
    });

    suite.add("memory faults stop the program precisely", [] {
        const PipelineRun load = runPipeline("addi x1,x0,256; lw x2,0[x1]; addi x3,x0,1");
        expect(load.result.outcome == rv::Outcome::Faulted && load.result.fault.has_value(), "faulted");
        if (load.result.fault) {
            expect(load.result.fault->kind == rv::FaultKind::LoadOutOfRange);
            expectEq(load.result.fault->pc, 4u);
            expectEq(load.result.fault->address, 256u);
        }
        expectEq(load.x(1), 256u);  // the older instruction completed
        expectEq(load.x(3), 0u);    // the younger one did not

        const PipelineRun misaligned = runPipeline("lw x2,2[x0]");
        expect(misaligned.result.fault && misaligned.result.fault->kind == rv::FaultKind::LoadMisaligned);
        const PipelineRun store = runPipeline("addi x1,x0,1; sw x1,256[x0]; sw x1,0[x0]");
        expect(store.result.fault && store.result.fault->kind == rv::FaultKind::StoreOutOfRange);
        expectEq(store.word(0), 0u);
        const PipelineRun half = runPipeline("sh x0,3[x0]");
        expect(half.result.fault && half.result.fault->kind == rv::FaultKind::StoreMisaligned);
    });

    suite.add("illegal instructions and bad jump targets fault", [] {
        const PipelineRun illegal{rv::Program{0x00100093u, 0xFFFFFFFFu, 0x00200113u}};
        expect(illegal.result.fault && illegal.result.fault->kind == rv::FaultKind::IllegalInstruction);
        expectEq(illegal.x(1), 1u);
        expectEq(illegal.x(2), 0u);

        // an illegal word on the wrong path of a taken branch is flushed, not executed
        const PipelineRun skipped{rv::Program{0x00000463u /*beq x0,x0,8*/, 0xFFFFFFFFu, 0x00200113u}};
        expect(skipped.result.outcome == rv::Outcome::Finished, "wrong-path word must not fault");
        expectEq(skipped.x(2), 2u);

        const PipelineRun outside = runPipeline("jalr x0,x0,400");
        expect(outside.result.fault && outside.result.fault->kind == rv::FaultKind::FetchOutOfRange);
        const PipelineRun unaligned = runPipeline("addi x1,x0,6; jalr x0,x1,0");
        expect(unaligned.result.fault && unaligned.result.fault->kind == rv::FaultKind::FetchMisaligned);
    });

    suite.add("cycle limit stops an infinite loop", [] {
        const PipelineRun run(assembleOrFail("loop: jal x0,loop"), 50);
        expect(run.result.outcome == rv::Outcome::CycleLimit);
        expectEq(run.result.stats.cycles, std::size_t{50});
        rv::Tracer quiet;
        const rv::Program program = assembleOrFail("loop: jal x0,loop");
        rv::Iss iss{rv::InstructionMemory(program)};
        expect(iss.run(quiet, 50).outcome == rv::Outcome::CycleLimit, "ISS");
    });

    suite.add("a disabled tracer receives nothing", [] {
        class Counting : public rv::Tracer {
        public:
            explicit Counting(bool on) : on_(on) {}
            bool enabled() const override { return on_; }
            void beginCycle(std::size_t) override { calls++; }
            void stage(std::string_view, std::string_view) override { calls++; }
            std::size_t calls = 0;

        private:
            bool on_;
        };
        const rv::Program program = assembleOrFail("addi x1,x0,1; add x2,x1,x1");
        for (const bool on : {false, true}) {
            Counting pipelineTracer(on), issTracer(on);
            rv::Pipeline pipeline{rv::InstructionMemory(program)};
            rv::Iss iss{rv::InstructionMemory(program)};
            pipeline.run(pipelineTracer);
            iss.run(issTracer);
            expect((pipelineTracer.calls != 0) == on && (issTracer.calls != 0) == on, on ? "enabled" : "disabled");
        }
    });

    suite.add("pipeline agrees with the ISS", [] {
        expectModelsAgree("addi x1,x0,5; addi x2,x0,0; loop: addi x2,x2,3; addi x1,x1,-1; bne x1,x0,loop; sw x2,4[x0]");
        expectModelsAgree("addi x1,x0,8; addi x4,x0,-2; sw x4,0[x1]; lw x5,0[x1]; lbu x6,0[x1]; add x7,x5,x6; sh x7,6[x1]");
        expectModelsAgree("addi x1,x0,1; addi x1,x1,1; addi x1,x1,1; add x2,x1,x1; addi x0,x0,5; add x3,x0,x2");
        expectModelsAgree("addi x1,x0,16; jalr x5,x1,0; addi x2,x0,1; addi x3,x0,2; addi x4,x5,4; jal x6,end; addi x7,x0,1; end:");
        expectModelsAgree("lui x1,0x12345; addi x1,x1,0x678; auipc x2,1; srai x3,x1,4; slt x4,x3,x1; sltu x5,x1,x3");
        expectModelsAgree("lw x1,0[x0]; beq x1,x0,a; addi x2,x0,1; a: lw x3,4[x0]; jalr x4,x3,24; addi x5,x0,1; ebreak");
        expectModelsAgree("addi x1,x0,1; sw x1,0[x0]; ecall; addi x2,x0,2");
        expectModelsAgree("addi x1,x0,252; lw x2,0[x1]; lh x3,2[x1]; lw x4,4[x1]");
        expectModelsAgree("addi x1,x0,-7; addi x2,x0,2; mul x3,x1,x2; mulh x4,x1,x2; mulhu x5,x1,x2; mulhsu x6,x1,x2");
        expectModelsAgree("addi x1,x0,-7; addi x2,x0,2; div x3,x1,x2; rem x4,x1,x2; divu x5,x1,x0; remu x6,x1,x0; sw x3,0[x0]");
        expectModelsAgree("addi x1,x0,3; loop: mul x2,x1,x1; sw x2,0[x0]; lw x3,0[x0]; div x4,x3,x1; addi x1,x1,-1; bne x1,x0,loop");
        expectModelsAgree("addi x1,x0,6; mul x2,x1,x1; lw x3,256[x0]; addi x4,x0,1");
        expectModelsAgree(rv::Program{0x00100093u, 0xFFFFFFFFu});
        expectModelsAgree("jal x0,40");
    });
}

}  // namespace test
