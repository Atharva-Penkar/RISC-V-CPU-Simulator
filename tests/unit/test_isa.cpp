#include "check.hpp"

#include <cstdint>

#include "rv/assembler.hpp"
#include "rv/decoder.hpp"

namespace test {

namespace {

rv::DecodedInst make(std::string_view mnemonic, unsigned rd, unsigned rs1, unsigned rs2, int32_t imm) {
    return {rv::findSpec(mnemonic), rv::Reg::fromField(rd), rv::Reg::fromField(rs1), rv::Reg::fromField(rs2), imm};
}

}  // namespace

void addIsaTests(Suite& suite) {
    suite.add("every mnemonic in the table is unique and found", [] {
        for (const rv::InstrSpec& spec : rv::kIsa)
            expect(rv::findSpec(spec.mnemonic) == &spec, spec.mnemonic);
        expect(rv::findSpec("LD") == nullptr, "RV64 LD must not exist");
        expect(rv::findSpec("add") == nullptr, "lookup expects upper case");
    });

    suite.add("Reg holds only x0..x31", [] {
        expect(rv::Reg::fromIndex(31).has_value());
        expect(!rv::Reg::fromIndex(32).has_value());
        expectEq(rv::Reg::fromField(0x3F).index(), 31u);
    });

    suite.add("signExtend and bits", [] {
        expectEq(rv::signExtend(0xFFF, 12), -1);
        expectEq(rv::signExtend(0x7FF, 12), 2047);
        expectEq(rv::signExtend(0x1000, 13), -4096);
        expectEq(rv::bits(0xABCD1234u, 15, 8), 0x12u);
    });

    suite.add("encodings match the LLVM RISC-V assembler", [] {
        expectEq(rv::encode(make("ADD", 1, 2, 3, 0)), 0x003100B3u);
        expectEq(rv::encode(make("SUB", 4, 5, 6, 0)), 0x40628233u);
        expectEq(rv::encode(make("XORI", 1, 2, 0, -10)), 0xFF614093u);
        expectEq(rv::encode(make("SRAI", 4, 3, 0, 4)), 0x4041D213u);
        expectEq(rv::encode(make("LW", 5, 1, 0, -4)), 0xFFC0A283u);
        expectEq(rv::encode(make("SW", 0, 1, 4, -8)), 0xFE40AC23u);
        expectEq(rv::encode(make("BNE", 0, 1, 2, -64)), 0xFC2090E3u);
        expectEq(rv::encode(make("BGEU", 0, 3, 4, 20)), 0x0041FA63u);
        expectEq(rv::encode(make("JAL", 1, 0, 0, -72)), 0xFB9FF0EFu);
        expectEq(rv::encode(make("LUI", 7, 0, 0, static_cast<int32_t>(0x80000000u))), 0x800003B7u);
    });

    suite.add("decode inverts encode for every instruction", [] {
        for (const rv::InstrSpec& spec : rv::kIsa) {
            rv::DecodedInst inst = make(spec.mnemonic, 5, 6, 7, 0);
            switch (rv::formatOf(spec.kind)) {
                case rv::Format::R: inst.imm = 0; break;
                case rv::Format::I: inst.imm = spec.kind == rv::Kind::Shift ? 13 : -77; inst.rs2 = {}; break;
                case rv::Format::S: inst.imm = -77; inst.rd = {}; break;
                case rv::Format::B: inst.imm = -78; inst.rd = {}; break;
                case rv::Format::U: inst.imm = static_cast<int32_t>(0xABCDE000u); inst.rs1 = inst.rs2 = {}; break;
                case rv::Format::J: inst.imm = -2046; inst.rs1 = inst.rs2 = {}; break;
            }
            const auto decoded = rv::decode(rv::encode(inst));
            expect(decoded.has_value(), spec.mnemonic);
            if (!decoded)
                continue;
            expect(decoded->spec == inst.spec, spec.mnemonic);
            expect(decoded->rd == inst.rd && decoded->rs1 == inst.rs1 && decoded->rs2 == inst.rs2, spec.mnemonic);
            expectEq(decoded->imm, inst.imm);
        }
    });

    suite.add("invalid words do not decode", [] {
        expect(!rv::decode(0x00000000u).has_value(), "all zero");
        expect(!rv::decode(0xFFFFFFFFu).has_value(), "all one");
        expect(!rv::decode(0x0000B003u).has_value(), "LD (RV64)");
    });

    suite.add("disassembly", [] {
        expectEq(rv::disassemble(*rv::decode(0xFFC0A283u)), std::string("LW x5, -4[x1]"));
        expectEq(rv::disassemble(*rv::decode(0xFC2090E3u)), std::string("BNE x1, x2, pc-64"));
        expectEq(rv::disassemble(*rv::decode(0x800003B7u)), std::string("LUI x7, 0x80000"));
    });
}

}  // namespace test
