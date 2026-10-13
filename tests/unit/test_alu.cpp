#include "check.hpp"

#include "rv/cpu.hpp"
#include "rv/decoder.hpp"

namespace test {

void addAluTests(Suite& suite) {
    using rv::AluOp;
    using rv::aluExecute;

    suite.add("arithmetic wraps", [] {
        expectEq(aluExecute(AluOp::Add, 0x7FFFFFFFu, 1), 0x80000000u);
        expectEq(aluExecute(AluOp::Sub, 0, 1), 0xFFFFFFFFu);
    });

    suite.add("shifts use the low five bits", [] {
        expectEq(aluExecute(AluOp::Sll, 1, 33), 2u);
        expectEq(aluExecute(AluOp::Srl, 0x80000000u, 31), 1u);
        expectEq(aluExecute(AluOp::Sra, 0x80000000u, 31), 0xFFFFFFFFu);
        expectEq(aluExecute(AluOp::Sra, 0x40000000u, 30), 1u);
    });

    suite.add("signed and unsigned compares", [] {
        const uint32_t minusOne = 0xFFFFFFFFu;
        expectEq(aluExecute(AluOp::Slt, minusOne, 0), 1u);
        expectEq(aluExecute(AluOp::Sltu, minusOne, 0), 0u);
        expectEq(aluExecute(AluOp::Lt, minusOne, 0), 1u);
        expectEq(aluExecute(AluOp::Ltu, minusOne, 0), 0u);
        expectEq(aluExecute(AluOp::Ge, 0, minusOne), 1u);
        expectEq(aluExecute(AluOp::Geu, 0, minusOne), 0u);
        expectEq(aluExecute(AluOp::Eq, 5, 5), 1u);
        expectEq(aluExecute(AluOp::Ne, 5, 5), 0u);
    });

    suite.add("multiply: low and high halves of signed and unsigned products", [] {
        const uint32_t minusTwo = 0xFFFFFFFEu;
        expectEq(aluExecute(AluOp::Mul, 6, 7), 42u);
        expectEq(aluExecute(AluOp::Mul, minusTwo, 3), 0xFFFFFFFAu);       // -6
        expectEq(aluExecute(AluOp::Mul, 0x10000u, 0x10000u), 0u);          // 2^32 wraps
        expectEq(aluExecute(AluOp::Mulh, 0x10000u, 0x10000u), 1u);
        expectEq(aluExecute(AluOp::Mulh, minusTwo, 3), 0xFFFFFFFFu);       // -6 >> 32 = -1
        expectEq(aluExecute(AluOp::Mulhu, minusTwo, 3), 2u);               // (2^32 - 2) * 3 >> 32
        expectEq(aluExecute(AluOp::Mulhsu, minusTwo, 3), 0xFFFFFFFFu);     // signed -2 times unsigned 3
        expectEq(aluExecute(AluOp::Mulhsu, 2, 0xFFFFFFFFu), 1u);           // 2 * (2^32 - 1) >> 32
        expectEq(aluExecute(AluOp::Mulh, 0x80000000u, 0x80000000u), 0x40000000u);
    });

    suite.add("divide: rounding toward zero, division by zero and overflow", [] {
        const uint32_t minusSeven = 0xFFFFFFF9u;
        expectEq(aluExecute(AluOp::Div, minusSeven, 2), 0xFFFFFFFDu);      // -3, not -4
        expectEq(aluExecute(AluOp::Rem, minusSeven, 2), 0xFFFFFFFFu);      // -1: the sign follows the dividend
        expectEq(aluExecute(AluOp::Divu, minusSeven, 2), 0x7FFFFFFCu);
        expectEq(aluExecute(AluOp::Remu, minusSeven, 2), 1u);
        expectEq(aluExecute(AluOp::Div, 5, 0), 0xFFFFFFFFu);
        expectEq(aluExecute(AluOp::Divu, 5, 0), 0xFFFFFFFFu);
        expectEq(aluExecute(AluOp::Rem, 5, 0), 5u);
        expectEq(aluExecute(AluOp::Remu, 5, 0), 5u);
        expectEq(aluExecute(AluOp::Div, 0x80000000u, 0xFFFFFFFFu), 0x80000000u);
        expectEq(aluExecute(AluOp::Rem, 0x80000000u, 0xFFFFFFFFu), 0u);
    });

    suite.add("M extension encodings", [] {
        const auto mul = rv::decode(0x022081B3u);   // mul x3, x1, x2
        expect(mul.has_value() && mul->spec->mnemonic == "MUL" && mul->rd.index() == 3);
        const auto remu = rv::decode(0x0220F1B3u);  // remu x3, x1, x2
        expect(remu.has_value() && remu->spec->mnemonic == "REMU");
        expect(rv::isMultiply(AluOp::Mulhsu) && !rv::isMultiply(AluOp::Div));
        expect(rv::isDivide(AluOp::Rem) && !rv::isDivide(AluOp::Add));
    });

    suite.add("control signals", [] {
        const rv::Control load = rv::controlFor(rv::Kind::Load);
        expect(load.memRead && load.memToReg && load.regWrite && load.readsRs1 && !load.readsRs2);
        const rv::Control store = rv::controlFor(rv::Kind::Store);
        expect(store.memWrite && !store.regWrite && store.readsRs1 && store.readsRs2);
        const rv::Control jal = rv::controlFor(rv::Kind::Jal);
        expect(jal.jump && jal.regWrite && !jal.readsRs1);
    });

    suite.add("execute computes link addresses and targets", [] {
        const auto jal = *rv::decode(0xFB9FF0EFu);  // jal x1, -72
        const rv::ExecuteResult j = rv::execute(jal, rv::controlFor(jal.kind()), 100, 0, 0);
        expectEq(j.value, 104u);
        expectEq(j.target.value_or(0), 28u);

        const auto jalr = *rv::decode(0xFFC100E7u);     // jalr x1, x2, -4
        const rv::ExecuteResult r = rv::execute(jalr, rv::controlFor(jalr.kind()), 8, 23, 0);
        expectEq(r.target.value_or(0), 18u);   // (23 - 4) & ~1

        const auto bne = *rv::decode(0xFC2090E3u);  // bne x1, x2, -64
        expect(!rv::execute(bne, rv::controlFor(bne.kind()), 64, 3, 3).target.has_value(), "not taken");
        expectEq(rv::execute(bne, rv::controlFor(bne.kind()), 64, 3, 4).target.value_or(1), 0u);
    });
}

}  // namespace test
