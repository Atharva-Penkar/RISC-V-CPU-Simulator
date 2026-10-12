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
        fail("assembly failed", where);
        return {};
    }
    return *program;
}

uint32_t reg(const rv::RegisterFile& regs, unsigned index) { return regs.read(rv::Reg::fromField(index)); }

/*counts cycles in which decode stalled*/
class StallCounter : public rv::Tracer {
public:
    void stage(std::string_view name, std::string_view description) override {
        if (name == "IF/ID" && description.find("STALL") != std::string_view::npos)
            stalls++;
    }
    std::size_t stalls = 0;
};

/*runs program on both models; the pipeline must end in the same state as the ISS*/
void expectModelsAgree(std::string_view source, std::source_location where = std::source_location::current()) {
    const rv::Program program = assembleOrFail(source, where);
    rv::Tracer quiet;
    rv::Iss iss{rv::InstructionMemory(program)};
    rv::Pipeline pipeline{rv::InstructionMemory(program)};
    const rv::RunResult a = iss.run(quiet);
    const rv::RunResult b = pipeline.run(quiet);
    if (!a.completed || !b.completed || a.retired != b.retired)
        fail("models retired different instruction counts", where);
    for (unsigned i = 0; i < 32; i++)
        if (reg(iss.registers(), i) != reg(pipeline.registers(), i))
            fail("register x" + std::to_string(i) + " differs between ISS and pipeline", where);
    for (uint32_t address = 0; address < iss.memory().size(); address += 4)
        if (iss.memory().read(address, 4) != pipeline.memory().read(address, 4))
            fail("memory word " + std::to_string(address) + " differs between ISS and pipeline", where);
}

}  // namespace

void addCpuTests(Suite& suite) {
    suite.add("hazard unit tracks the youngest writer", [] {
        rv::HazardUnit hazards;
        const auto addi = *rv::decode(0x00100093u);    // addi x1, x0, 1
        const auto add = *rv::decode(0x001080B3u);     // add x1, x1, x1
        const rv::Control cw = rv::controlFor(rv::Kind::Alu);
        expect(!hazards.mustStall(add, cw), "nothing pending");
        hazards.issue(addi, rv::controlFor(addi.kind()), 1);
        expect(hazards.mustStall(add, cw), "x1 pending");
        hazards.issue(add, cw, 2);
        hazards.writeBack(addi.rd, 1);
        expect(hazards.mustStall(add, cw), "older writer must not clear a younger reservation");
        hazards.writeBack(add.rd, 2);
        expect(!hazards.mustStall(add, cw), "cleared");
    });

    suite.add("pipeline stalls on a read-after-write dependency", [] {
        const rv::Program dependent = assembleOrFail("addi x1,x0,1; add x2,x1,x1");
        const rv::Program independent = assembleOrFail("addi x1,x0,1; add x2,x3,x3");
        StallCounter a, b;
        rv::Pipeline p1{rv::InstructionMemory(dependent)};
        rv::Pipeline p2{rv::InstructionMemory(independent)};
        const rv::RunResult r1 = p1.run(a);
        const rv::RunResult r2 = p2.run(b);
        expect(a.stalls > 0 && b.stalls == 0, "stall only for the dependent pair");
        expectEq(r1.cycles, r2.cycles + a.stalls);
        expectEq(reg(p1.registers(), 2), 2u);
    });

    suite.add("taken branch flushes the wrong-path instruction", [] {
        const rv::Program program = assembleOrFail("beq x0,x0,skip; addi x1,x0,1; skip: addi x2,x0,2");
        rv::Tracer quiet;
        rv::Pipeline pipeline{rv::InstructionMemory(program)};
        const rv::RunResult result = pipeline.run(quiet);
        expectEq(reg(pipeline.registers(), 1), 0u);
        expectEq(reg(pipeline.registers(), 2), 2u);
        expectEq(result.retired, std::size_t{2});
    });

    suite.add("cycle limit stops an infinite loop", [] {
        const rv::Program program = assembleOrFail("loop: jal x0,loop");
        rv::Tracer quiet;
        rv::Pipeline pipeline{rv::InstructionMemory(program)};
        const rv::RunResult result = pipeline.run(quiet, 50);
        expect(!result.completed);
        expectEq(result.cycles, std::size_t{50});
    });

    suite.add("pipeline agrees with the ISS", [] {
        expectModelsAgree("addi x1,x0,5; addi x2,x0,0; loop: addi x2,x2,3; addi x1,x1,-1; bne x1,x0,loop; sw x2,4[x0]");
        expectModelsAgree("addi x1,x0,8; addi x4,x0,-2; sw x4,0[x1]; lw x5,0[x1]; lbu x6,0[x1]; add x7,x5,x6; sh x7,6[x1]");
        expectModelsAgree("addi x1,x0,1; addi x1,x1,1; addi x1,x1,1; add x2,x1,x1; addi x0,x0,5; add x3,x0,x2");
        expectModelsAgree("addi x1,x0,16; jalr x5,x1,0; addi x2,x0,1; addi x3,x0,2; addi x4,x5,4; jal x6,end; addi x7,x0,1; end:");
        expectModelsAgree("lui x1,0x12345; addi x1,x1,0x678; auipc x2,1; srai x3,x1,4; slt x4,x3,x1; sltu x5,x1,x3");
    });
}

}  // namespace test
