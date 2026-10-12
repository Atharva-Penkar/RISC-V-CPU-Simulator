# RISC-V CPU Simulator

Simulation of a RISC-V architechture CPU. The code contain the implementation of an assembler to covert RISC-V assembly to binary.This project is a simulation of a **5-stage pipelined RISC-V 32-bit CPU** written in C++, along with a custom assembler that converts RISC-V assembly code to binary (little-endian format). It models the core functionalities of a RISC-V processor, including instruction execution, pipeline flow, and register/memory handling.

## 5 Stage Implementation
- Fetch
- Decode
- Execute
- Memory (RAM) access
- Write Back

Handling of data hazards is done using stall mechanism. The CPU waits for the write back to happen before reading value from the current register or memory, to avoid data inconsistency.

Compile the code with a C++20 compiler (GCC 10+, Clang 14+ or MSVC 19.29+):

```
g++ -std=c++20 -O2 22cs02011_assm_and_cpu.cpp -o 22cs02011_assm_and_cpu.exe
./22cs02011_assm_and_cpu.exe
```

With `-std=c++23` the assembler's error reporting uses `std::expected`; with C++20 it uses an equivalent built-in `Result` type.

Enter instructions separated by `;` or new lines, and finish with an empty line, for example:

```
addi x1,x0,5;
loop: addi x2,x2,1; bne x2,x1,loop;
sw x2,0x0[x0];

```

The assembler reports every error in the input with its line and column before anything is run.
