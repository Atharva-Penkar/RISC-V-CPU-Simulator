# RISC-V CPU Simulator

An assembler and a **5-stage pipelined RISC-V (RV32IM) CPU** simulator written in C++20.

**Try it in the browser: https://atharva-penkar.github.io/RISC-V-CPU-Simulator/**

- `rvasm` turns RV32IM assembly into machine code and reports every error with its line and column.
- `rvsim` runs a program on the pipeline, or on a single-cycle reference model (ISS), and prints the final
  registers and data memory. With `--json` it records every pipeline cycle as structured JSON for visualisation
  ([format](docs/timeline-format.md)).
- `backend/` is a Node.js API that accepts programs from a web frontend, runs them on `rvsim` and returns the
  timeline JSON ([API](backend/README.md)).
- `frontend/` is the React website that shows the pipeline cycle by cycle
  ([live site](https://atharva-penkar.github.io/RISC-V-CPU-Simulator/), [frontend README](frontend/README.md)).

## Pipeline

| Stage | Work |
| --- | --- |
| IF  | fetch the instruction at PC, PC += 4 |
| ID  | decode, read registers, detect load-use hazards |
| EX  | ALU operation with forwarded operands; branches and jumps resolve here |
| MEM | data memory load / store; faults and halts take effect here |
| WB  | register write back (first half of the cycle, so ID reads the new value) |

Each cycle, every stage works on the pipeline registers as they were at the start of the cycle, and all the new
values are applied together at the end, like a clock edge.

- **Forwarding:** the hazard unit forwards results into EX from EX/MEM (the previous instruction) and MEM/WB (two
  back), so dependent ALU instructions, branches, jump targets and store data run without stalling.
- **Load-use stall:** an instruction that needs the result of the load directly ahead of it waits one cycle. A
  bubble enters ID/EX, then MEM/WB forwarding supplies the loaded value.
- **Branches and jumps:** they resolve in EX. When one is taken, the instructions in IF/ID and ID/EX are on the
  wrong path and are flushed, so a taken branch costs two cycles.
- **Halting:** `ecall` and `ebreak` stop the program once every older instruction has completed. Running past the
  last instruction also ends the program.
- **Faults:** a load or store outside data memory or not naturally aligned, an illegal instruction word, or a jump
  outside the program raises a fault and stops the program. Faults are precise: older instructions complete,
  younger ones have no effect, and an illegal word on a flushed wrong path never faults.
- **Multi-cycle operations:** by default every stage takes one cycle. `--mul-latency`, `--div-latency`,
  `--load-latency` and `--store-latency` (1 to 100) make multiplies and divides spend that many cycles in EX, and
  loads and stores in MEM. The busy stage keeps its instruction, everything behind it waits, and a bubble moves on,
  so a slow divider or memory shows up directly in the cycle count and CPI.
- **Forwarding switch:** `--no-forwarding` removes the forwarding paths, so an instruction waits in ID until every
  older instruction it reads from has written back (two cycles behind its neighbour, one cycle two behind).
  Comparing the cycle counts with and without it shows what forwarding saves.
- **Cycle limit:** a program stops after 10000 cycles by default (`--max-cycles`), so an infinite loop can't hang
  the simulator.

The data memory is 256 bytes, byte-addressed and little-endian. `lb`/`lh`/`lw` read 1, 2 or 4 bytes and
sign-extend; `lbu`/`lhu` zero-extend.

After a run, `rvsim` prints the final registers and memory, followed by statistics:

```
statistics
  outcome       finished
  cycles        19
  instructions  11
  CPI           1.73
  stalls        2 cycles (data hazards)
  flushes       2 instructions (1 taken branch/jump)
```

The reference model (`--model iss`) runs one instruction per cycle, so its CPI is always 1.00. Comparing the two
shows what the hazards cost.

The program is decoded once when it is loaded, and trace text is only built when `--trace` is on. Without tracing,
a Release build runs about 12 million pipeline cycles per second.

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
build/rvsim --trace tests/programs/forwarding.s       # pipeline registers, stalls, flushes and forwarding per cycle
build/rvsim --model iss fib.hex                       # reference model, machine code input
build/rvsim --json tests/programs/forwarding.s        # every cycle as JSON (pipeline model)
build/rvsim --mul-latency 3 --div-latency 32 tests/programs/multiply.s   # slow multiplier and divider
build/rvsim --no-forwarding tests/programs/forwarding.s   # every dependency waits for write back
build/rvasm --json tests/programs/forwarding.s        # instructions with their source map
```

Without a file argument, both tools read standard input. `rvsim` exits with status 0 when the program finishes or
halts, 1 for bad input, 2 when the cycle limit stops it, and 3 on a fault. `--no-stats` prints only the final state.

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
- **Instructions:** the RV32I base integer instructions except `fence`, and the M extension (`mul`, `mulh`,
  `mulhsu`, `mulhu`, `div`, `divu`, `rem`, `remu`). `ecall` and `ebreak` halt the program. Division by zero does not
  trap: the quotient is all ones and the remainder is the dividend, as the ISA specifies.

## Project layout

```
include/rv/   isa.hpp        instruction table, Reg, DecodedInst (shared by everything)
              assembler.hpp  assemble(), encode(), Diagnostic, Result
              decoder.hpp    decode(), disassemble()
              memory.hpp     RegisterFile, InstructionMemory (decoded at load), DataMemory
              cpu.hpp        Control, ALU, faults, HazardUnit, Iss, Pipeline, run statistics
              trace.hpp      Tracer interface, per-cycle text trace, state dump
              timeline.hpp   structured per-cycle record of a pipeline run (Timeline, recordPipeline)
              json.hpp       JSON documents for the timeline, programs and assembly errors
src/asm/      lexer.cpp      source -> statements and tokens
              parser.cpp     operands and labels -> DecodedInst; the two assembler passes
              encoder.cpp    DecodedInst -> 32-bit word
src/sim/      decoder.cpp    32-bit word -> DecodedInst, disassembly
              alu.cpp        control signals, ALU, execute and memory stage logic, faults
              memory.cpp     data memory loads, stores and bounds checks
              iss.cpp        single-cycle reference model
              pipeline.cpp   5-stage pipeline
              hazard.cpp     forwarding selection and load-use detection
              timeline.cpp   recording a run, replaying state at any cycle
              json.cpp       JSON serialisation
apps/         rvasm.cpp, rvsim.cpp   command line tools
backend/      Node.js API for the web frontend
docs/         timeline-format.md     the JSON timeline format
tests/unit/   unit tests (no external dependencies)
tests/programs/  *.s programs with the expected final state (*.expected), run on both CPU models
```

## Tests

`ctest` runs the following:
- **Unit tests:**
  - encodings checked against the LLVM RISC-V assembler, and encode/decode round trips for every instruction;
  - the assembler's diagnostics, the ALU, memory and the hazard unit;
  - exact pipeline cycle, stall and flush counts, plus faults and halts;
  - pipeline-versus-ISS comparisons.
- **Program tests:** every program in `tests/programs` on both the pipeline and the ISS, compared with its
  `.expected` dump.

To add a test program, write `name.s`, create `name.expected` with `rvsim --model iss --no-stats name.s > name.expected`, and
check the values by hand.
