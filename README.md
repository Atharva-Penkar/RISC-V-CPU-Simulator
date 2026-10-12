# RISC-V CPU Simulator

An assembler and a **5-stage pipelined RISC-V (RV32I) CPU** simulator written in C++20.

- `rvasm` turns RV32I assembly into machine code and reports every error with its line and column.
- `rvsim` runs a program on the pipeline, or on a single-cycle reference model (ISS), and prints the final
  registers and data memory.

## Pipeline

| Stage | Work |
| --- | --- |
| IF  | fetch the instruction at PC, PC += 4 |
| ID  | decode, read registers, stall on data hazards |
| EX  | ALU operation; branches and jumps resolve here |
| MEM | data memory load / store |
| WB  | register write back |

Data hazards are handled by stalling. A scoreboard (`HazardUnit`) marks the destination register of every issued
instruction, and decode waits until the write back of any source register has happened. A taken branch or jump
resolves in EX and flushes the one instruction fetched behind it.

The data memory is 256 bytes, byte-addressed and little-endian. Accesses must be naturally aligned.

## Building

You need CMake 3.20 or newer and a C++20 compiler (GCC 10+, Clang 14+, MSVC 19.29+).

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Build options:
- `-DCMAKE_CXX_STANDARD=23`: use `std::expected` for error results; C++20 uses an equivalent built-in type.
- `-DRV_WARNINGS_AS_ERRORS=ON`: treat compiler warnings as errors.
- `-DRV_SANITIZE=ON`: build with AddressSanitizer and UndefinedBehaviorSanitizer (GCC/Clang).

## Usage

```sh
build/rvasm tests/programs/fibonacci.s                # one hex word per line
build/rvasm --listing tests/programs/fibonacci.s      # address, encoding, disassembly
build/rvasm -o fib.hex tests/programs/fibonacci.s

build/rvsim tests/programs/fibonacci.s                # final state on the pipeline
build/rvsim --trace tests/programs/hazards.s          # pipeline registers after every cycle
build/rvsim --model iss fib.hex                       # reference model, machine code input
```

Without a file argument, both tools read standard input. `rvsim` exits with status 0 when the program finishes,
1 for bad input, and 2 when the cycle limit stops it (`--max-cycles`, default 10000).

## Assembly syntax

```asm
# Fibonacci numbers at addresses 0, 4, 8, ...
        addi x1, x0, 0
        addi x2, x0, 1
        addi x3, x0, 0
        addi x4, x0, 0x30
next:   sw   x1, 0[x3]          # memory operands: imm[reg] or imm(reg)
        add  x5, x1, x2; addi x1, x2, 0; addi x2, x5, 0
        addi x3, x3, 4
        blt  x3, x4, next
```

- **Statements:** end at a new line or at `;`. `#` starts a comment.
- **Labels:** `name:`, either on their own or in front of an instruction. Branch and `jal` targets are labels (or
  even byte offsets). In `jalr rd, rs1, label`, the label stands for its absolute address.
- **Registers:** `x0` to `x31`.
- **Immediates:** decimal (`-2`) or hexadecimal (`0xF`). Hexadecimal is read as 32-bit two's complement, so
  `0xFFFFFFFE` is -2. `lui` and `auipc` take the 20-bit upper value.
- **Case:** mnemonics, registers and labels are case-insensitive.
- **Instructions:** the RV32I base integer instructions except `fence`, `ecall` and `ebreak`.

## Project layout

```
include/rv/   isa.hpp        instruction table, Reg, DecodedInst (shared by everything)
              assembler.hpp  assemble(), encode(), Diagnostic, Result
              decoder.hpp    decode(), disassemble()
              memory.hpp     RegisterFile, InstructionMemory, DataMemory
              cpu.hpp        Control, ALU, HazardUnit, Iss, Pipeline
              trace.hpp      Tracer interface, text tracer, state dump
src/asm/      lexer.cpp      source -> statements and tokens
              parser.cpp     operands and labels -> DecodedInst; the two assembler passes
              encoder.cpp    DecodedInst -> 32-bit word
src/sim/      decoder.cpp    32-bit word -> DecodedInst, disassembly
              alu.cpp        control signals, ALU, execute stage
              memory.cpp     data memory loads and stores
              iss.cpp        single-cycle reference model
              pipeline.cpp   5-stage pipeline
              hazard.cpp     scoreboard for RAW hazards
apps/         rvasm.cpp, rvsim.cpp   command line tools
tests/unit/   unit tests (no external dependencies)
tests/programs/  *.s programs with the expected final state (*.expected), run on both CPU models
```

## Tests

`ctest` runs the following:
- **Unit tests:** encodings checked against the LLVM RISC-V assembler, encode/decode round trips for every
  instruction, diagnostics, the ALU, memory, the hazard unit, and pipeline-versus-ISS comparisons.
- **Program tests:** every program in `tests/programs` on both the pipeline and the ISS, compared with its
  `.expected` dump.

To add a test program, write `name.s`, create `name.expected` with `rvsim --model iss name.s > name.expected`, and
check the values by hand.
