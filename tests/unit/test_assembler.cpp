#include "check.hpp"

#include <cstdint>
#include <vector>

#include "rv/assembler.hpp"

namespace test {

namespace {

std::vector<uint32_t> assembleOk(std::string_view source, std::source_location where = std::source_location::current()) {
    auto program = rv::assemble(source);
    if (!program) {
        for (const rv::Diagnostic& d : program.error())
            std::cerr << "  " << d.line << ':' << d.column << ": " << d.message << '\n';
        fail("assembly failed", where);
        return {};
    }
    return *program;
}

std::vector<rv::Diagnostic> assembleErrors(std::string_view source, std::source_location where = std::source_location::current()) {
    auto program = rv::assemble(source);
    if (program) {
        fail("assembly unexpectedly succeeded", where);
        return {};
    }
    return program.error();
}

void expectError(const rv::Diagnostic& d, std::size_t line, std::size_t column, std::string_view fragment,
                 std::source_location where = std::source_location::current()) {
    if (d.line != line || d.column != column || d.message.find(fragment) == std::string::npos) {
        std::cerr << "  got " << d.line << ':' << d.column << ": " << d.message << "\n  want " << line << ':' << column
                  << ": ..." << fragment << "...\n";
        fail("unexpected diagnostic", where);
    }
}

}  // namespace

void addAssemblerTests(Suite& suite) {
    suite.add("program matches the LLVM RISC-V assembler", [] {
        const auto program = assembleOk(
            "start: add x1,x2,x3;sub x4,x5,x6;sra x7,x8,x9;sltu x10,x11,x12;xori x1,x2,-10;srai x4,x3,4;"
            "slli x6,x3,31;lw x5,-4[x1];lbu x6,0x7FF[x31];lhu x1,2[x2];sw x4,-8[x1];sb x31,5[x2];sh x1,0x10[x3];"
            "lui x7,0x80000;auipc x2,0xFFFFF;loop: beq x1,x2,loop;bne x1,x2,start;bgeu x3,x4,fwd;jal x1,start;"
            "jal x0,fwd;jalr x1,x2,-4;addi x0,x0,0;fwd: blt x5,x6,start;");
        const std::vector<uint32_t> llvm = {
            0x003100b3, 0x40628233, 0x409453b3, 0x00c5b533, 0xff614093, 0x4041d213, 0x01f19313, 0xffc0a283,
            0x7fffc303, 0x00215083, 0xfe40ac23, 0x01f102a3, 0x00119823, 0x800003b7, 0xfffff117, 0x00208063,
            0xfc2090e3, 0x0041fa63, 0xfb9ff0ef, 0x00c0006f, 0xffc100e7, 0x00000013, 0xfa62c4e3};
        expect(program == llvm, "encodings differ from llvm-mc -triple=riscv32");
    });

    suite.add("separators, comments, labels and case", [] {
        const auto a = assembleOk("ADDI x1, x0, 5   # comment\nLoop: addi x2,x2,1; BNE x2, X1, LOOP\n\n");
        const auto b = assembleOk("addi x1,x0,5;loop:;addi x2,x2,1;bne x2,x1,loop;");
        expect(a == b && a.size() == 3, "equivalent sources assemble differently");
    });

    suite.add("ecall and ebreak take no operands", [] {
        expect(assembleOk("ecall; EBREAK") == std::vector<uint32_t>{0x00000073u, 0x00100073u}, "encodings");
        expect(assembleErrors("ecall x1").size() == 1, "operand rejected");
    });

    suite.add("immediate formats", [] {
        expect(assembleOk("addi x1,x0,0xF") == assembleOk("addi x1,x0,15"), "0xF");
        expect(assembleOk("addi x1,x0,0xFFFFFFFE") == assembleOk("addi x1,x0,-2"), "32-bit hex is two's complement");
        expect(assembleOk("addi x1,x0,0xFFF") == assembleOk("addi x1,x0,-1"), "12-bit pattern");
        expect(assembleOk("lw x1,0(x2)") == assembleOk("lw x1,[x2]"), "memory operand forms");
    });

    suite.add("labels point at the next instruction", [] {
        // a label-only line is not an instruction: the branch at index 2 targets index 1
        const auto program = assembleOk("addi x1,x0,1\nhere:\naddi x1,x1,1\nbeq x0,x0,here");
        expectEq(program.size(), std::size_t{3});
        expectEq(program[2], 0xFE000EE3u);    // beq x0,x0,-4
    });

    suite.add("all errors are reported with line and column", [] {
        const auto errors = assembleErrors(
            "addi x1, x0, 4; foo x1,x2\n"
            "add x1,x2;lw x3,8[x40]; beq x1,x2,nowhere\n"
            "1bad: sd x1,0[x2]; addi x1,x0,4096;slli x1,x1,32\n"
            "loop:; loop: lui x1,0x100000;sw x1,8x2\n");
        expect(errors.size() == 11, "expected 11 diagnostics");
        if (errors.size() != 11)
            return;
        expectError(errors[0], 1, 17, "unknown instruction 'foo'");
        expectError(errors[1], 2, 1, "expects 3 operands");
        expectError(errors[2], 2, 19, "invalid register 'x40'");
        expectError(errors[3], 2, 35, "undefined label 'nowhere'");
        expectError(errors[4], 3, 1, "invalid label name");
        expectError(errors[5], 3, 7, "unknown instruction 'sd'");
        expectError(errors[6], 3, 31, "does not fit in 12 bits");
        expectError(errors[7], 3, 47, "shift amount");
        expectError(errors[8], 4, 8, "defined more than once");
        expectError(errors[9], 4, 21, "does not fit in 20 bits");
        expectError(errors[10], 4, 36, "invalid memory operand");
    });

    suite.add("range checks", [] {
        expect(assembleErrors("addi x1,x0,-2049").size() == 1, "12-bit lower bound");
        expect(assembleErrors("beq x0,x0,3").size() == 1, "odd branch offset");
        expect(assembleErrors("beq x0,x0,4096").size() == 1, "branch range");
        expect(assembleErrors("addi x1,x0,0x123456789").size() == 1, "more than 8 hex digits");
    });

    suite.add("formatDiagnostic points at the column", [] {
        const std::string text = rv::formatDiagnostic({2, 5, "bad"}, "nop\nadd x1\n", "f.s");
        expectEq(text, std::string("f.s:2:5: error: bad\n    add x1\n        ^\n"));
    });
}

}  // namespace test
