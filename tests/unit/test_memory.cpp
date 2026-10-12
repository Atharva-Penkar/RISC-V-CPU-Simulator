#include "check.hpp"

#include "rv/memory.hpp"

namespace test {

void addMemoryTests(Suite& suite) {
    suite.add("register file ignores writes to x0", [] {
        rv::RegisterFile regs;
        regs.write(rv::Reg::fromField(0), 42);
        regs.write(rv::Reg::fromField(5), 42);
        expectEq(regs.read(rv::Reg::fromField(0)), 0u);
        expectEq(regs.read(rv::Reg::fromField(5)), 42u);
    });

    suite.add("little-endian loads with sign and zero extension", [] {
        rv::DataMemory memory(16);
        expect(memory.store(0b010, 4, 0x8001FFFEu), "SW");
        expectEq(memory.read(4, 1).value_or(0), 0xFEu);
        expectEq(memory.load(0b000, 4).value_or(0), 0xFFFFFFFEu);    // LB
        expectEq(memory.load(0b100, 4).value_or(0), 0xFEu);          // LBU
        expectEq(memory.load(0b001, 6).value_or(0), 0xFFFF8001u);    // LH
        expectEq(memory.load(0b101, 6).value_or(0), 0x8001u);        // LHU
        expectEq(memory.load(0b010, 4).value_or(0), 0x8001FFFEu);    // LW
        expect(memory.store(0b001, 4, 0x1234), "SH");
        expectEq(memory.load(0b010, 4).value_or(0), 0x80011234u);
    });

    suite.add("misaligned and out of range accesses fail", [] {
        rv::DataMemory memory(16);
        expect(!memory.load(0b010, 2).has_value(), "misaligned word");
        expect(!memory.load(0b001, 1).has_value(), "misaligned half");
        expect(!memory.load(0b010, 16).has_value(), "past the end");
        expect(!memory.load(0b010, 0xFFFFFFFCu).has_value(), "wraps around");
        expect(!memory.store(0b000, 16, 1), "store past the end");
    });

    suite.add("instruction memory bounds", [] {
        const std::vector<uint32_t> words = {1, 2};
        const rv::InstructionMemory imem(words);
        expect(imem.contains(4) && !imem.contains(8) && !imem.contains(2));
        expectEq(imem.fetch(4), 2u);
        expectEq(imem.endAddress(), 8u);
    });
}

}  // namespace test
