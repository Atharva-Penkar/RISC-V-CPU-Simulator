// CPU models: control and ALU, the hazard unit, a single-cycle instruction set simulator (ISS)
// used as the reference model, and the 5-stage pipeline.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "rv/isa.hpp"
#include "rv/memory.hpp"
#include "rv/trace.hpp"

namespace rv {

inline constexpr std::size_t kMaxCycles = 10000;    // guards against programs that never terminate

struct Control {
    bool readsRs1 = false;
    bool readsRs2 = false;
    bool aluSrc = false;    // second ALU operand is the immediate
    bool regWrite = false;
    bool memRead = false;
    bool memWrite = false;
    bool memToReg = false;
    bool branch = false;
    bool jump = false;
};

Control controlFor(Kind kind);
std::string toString(const Control& control);

uint32_t aluExecute(AluOp op, uint32_t a, uint32_t b);

struct ExecuteResult {
    uint32_t value = 0;                 // memory address for loads/stores, otherwise the value to write back
    std::optional<uint32_t> target;     // set when a branch is taken or for a jump
};

/*the execute stage, shared by both CPU models*/
ExecuteResult execute(const DecodedInst& inst, const Control& control, uint32_t pc, uint32_t rs1, uint32_t rs2);

/*scoreboard: a register is pending while an issued instruction will still write it*/
class HazardUnit {
public:
    /*true when a source register of inst still waits for its write back (RAW hazard)*/
    bool mustStall(const DecodedInst& inst, const Control& control) const;
    /*inst enters the pipeline with this tag*/
    void issue(const DecodedInst& inst, const Control& control, uint64_t tag);
    /*the instruction with this tag has written rd; only the youngest writer clears the reservation*/
    void writeBack(Reg rd, uint64_t tag);

private:
    std::array<std::optional<uint64_t>, 32> pending_{};
};

struct RunResult {
    std::size_t cycles = 0;
    std::size_t retired = 0;
    bool completed = false;     // false when the cycle limit stopped the program
};

/*executes one instruction per step; the reference the pipeline is tested against*/
class Iss {
public:
    explicit Iss(InstructionMemory program) : imem_(program) {}

    RunResult run(Tracer& tracer, std::size_t maxInstructions = kMaxCycles);

    const RegisterFile& registers() const { return regs_; }
    const DataMemory& memory() const { return dmem_; }

private:
    InstructionMemory imem_;
    RegisterFile regs_;
    DataMemory dmem_;
};

/*5-stage pipeline (IF, ID, EX, MEM, WB). RAW hazards stall in ID until the producer has written
back; branches and jumps resolve in EX and flush the one instruction fetched behind them*/
class Pipeline {
public:
    explicit Pipeline(InstructionMemory program);

    RunResult run(Tracer& tracer, std::size_t maxCycles = kMaxCycles);

    const RegisterFile& registers() const { return regs_; }
    const DataMemory& memory() const { return dmem_; }

private:
    struct ProgramCounter {
        uint32_t value = 0;
        bool valid = false;
    };

    struct IfId {
        bool valid = false;
        bool stall = false;
        uint32_t pc = 0;
        uint32_t ir = 0;
    };

    struct IdEx {
        bool valid = false;
        DecodedInst inst;
        Control cw;
        uint32_t pc = 0;
        uint32_t rs1 = 0;   // operand values
        uint32_t rs2 = 0;
        uint64_t tag = 0;
    };

    struct ExMem {
        bool valid = false;
        DecodedInst inst;
        Control cw;
        uint32_t aluOut = 0;    // memory address, or the value to write back
        uint32_t storeData = 0;
        uint64_t tag = 0;
    };

    struct MemWb {
        bool valid = false;
        DecodedInst inst;
        Control cw;
        uint32_t aluOut = 0;
        uint32_t loadData = 0;
        uint64_t tag = 0;
    };

    void fetch();
    void decodeStage(Tracer& tracer);
    void executeStage(Tracer& tracer);
    void memoryStage(Tracer& tracer);
    void writeBackStage();
    void traceState(Tracer& tracer) const;
    bool busy() const;

    InstructionMemory imem_;
    RegisterFile regs_;
    DataMemory dmem_;
    HazardUnit hazards_;
    ProgramCounter pc_;
    IfId ifid_;
    IdEx idex_;
    ExMem exmem_;
    MemWb memwb_;
    uint64_t nextTag_ = 0;
    std::size_t retired_ = 0;
};

}  // namespace rv
