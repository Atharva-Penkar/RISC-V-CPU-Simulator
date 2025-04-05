# RISC-V CPU Simulator

Simulation of a RISC-V architechture CPU. The code contain the implementation of an assembler to covert RISC-V assembly to binary.This project is a simulation of a **5-stage pipelined RISC-V 32-bit CPU** written in C++, along with a custom assembler that converts RISC-V assembly code to binary (little-endian format). It models the core functionalities of a RISC-V processor, including instruction execution, pipeline flow, and register/memory handling.

## 5 Stage Implementation
- Fetch
- Decode
- Execute
- Memory (RAM) access
- Write Back

Handling of data hazards is done using stall mechanism. The CPU waits for the write back to happen before reading value from the current register or memory, to avoid data inconsistency.

compile the code using
'''g++ 22cs02011_assm_and_cpu.cpp -o 22cs02011_assm_and_cpu.exe'''
'''./22cs02011_assm_and_cpu.exe'''
