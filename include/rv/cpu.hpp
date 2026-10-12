// CPU models: control and ALU, faults, the hazard unit, a single-cycle instruction set simulator (ISS)
// used as the reference model, and the 5-stage pipeline.
#pragma once

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
    bool halt = false;      // ECALL, EBREAK
};

Control controlFor(Kind kind);
std::string toString(const Control& control);

uint32_t aluExecute(AluOp op, uint32_t a, uint32_t b);

struct ExecuteResult {
    uint32_t value = 0;                 // memory address for loads/stores, otherwise the value to write back
    std::optional<uint32_t> target;     // set when a branch is taken or for a jump
};

/*the execute stage, shared by both CPU models; rs1 and rs2 are the operand values*/
ExecuteResult execute(const DecodedInst& inst, const Control& control, uint32_t pc, uint32_t rs1, uint32_t rs2);

/*an exception that stops the program; the faulting instruction does not complete*/
enum class FaultKind : uint8_t {
    IllegalInstruction,
    FetchOutOfRange,
    FetchMisaligned,
    LoadOutOfRange,
    LoadMisaligned,
    StoreOutOfRange,
    StoreMisaligned,
};

struct Fault {
    FaultKind kind;
    uint32_t pc;        // address of the faulting instruction (for fetch faults, the address fetched)
    uint32_t address;   // the bad memory or fetch address, or the instruction word for an illegal instruction
};

std::string describe(const Fault& fault);

/*the memory stage, shared by both CPU models: the loaded value (0 for other instructions) or a fault*/
struct MemoryResult {
    uint32_t loaded = 0;
    std::optional<Fault> fault;
};
MemoryResult accessMemory(DataMemory& memory, const DecodedInst& inst, const Control& control, uint32_t pc,
                          uint32_t address, uint32_t storeData);

/*where the execute stage takes a source operand from*/
enum class ForwardSource : uint8_t { RegisterFile, ExMem, MemWb };

/*a pipeline register's pending register write, as the hazard unit sees it*/
struct PendingWrite {
    bool valid = false;     // the pipeline register holds an instruction that writes rd
    Reg rd;
};

/*data-hazard logic of the pipeline*/
class HazardUnit {
public:
    /*the newest older instruction that writes source wins; x0 is never forwarded*/
    static ForwardSource forward(Reg source, const PendingWrite& exMem, const PendingWrite& memWb);

    /*the instruction in ID needs a register the load in EX has not read from memory yet:
    it must wait one cycle, after which MEM/WB forwarding supplies the value*/
    static bool loadUseHazard(const DecodedInst& inId, const Control& idControl, const PendingWrite& loadInEx);
};

enum class Outcome : uint8_t {
    Finished,       // ran past the last instruction
    Halted,         // ECALL or EBREAK
    Faulted,
    CycleLimit,     // stopped by the cycle limit, possibly an infinite loop
};

std::string toString(Outcome outcome);

struct RunStats {
    std::size_t cycles = 0;
    std::size_t retired = 0;        // instructions that completed
    std::size_t stalls = 0;         // cycles a load-use hazard held the pipeline
    std::size_t flushes = 0;        // wrong-path instructions discarded
    std::size_t redirects = 0;      // taken branches and jumps

    double cpi() const { return retired == 0 ? 0.0 : static_cast<double>(cycles) / static_cast<double>(retired); }
};

struct RunResult {
    Outcome outcome = Outcome::Finished;
    RunStats stats;
    std::optional<Fault> fault;     // set when outcome is Faulted
    uint32_t haltPc = 0;            // address of the ECALL/EBREAK when outcome is Halted
};

/*executes one instruction per cycle; the reference the pipeline is tested against*/
class Iss {
public:
    explicit Iss(InstructionMemory program) : imem_(program) {}

    RunResult run(Tracer& tracer, std::size_t maxCycles = kMaxCycles);

    const RegisterFile& registers() const { return regs_; }
    const DataMemory& memory() const { return dmem_; }

private:
    InstructionMemory imem_;
    RegisterFile regs_;
    DataMemory dmem_;
};

/*5-stage pipeline (IF, ID, EX, MEM, WB) with full forwarding into EX, a one-cycle stall for a load
followed by a dependent instruction, and branches and jumps resolved in EX, which flush IF/ID and ID/EX.
Faults and halts take effect when the instruction reaches MEM, so they are precise: every older
instruction completes and no younger one has changed any state*/
class Pipeline {
public:
    explicit Pipeline(InstructionMemory program);

    RunResult run(Tracer& tracer, std::size_t maxCycles = kMaxCycles);

    const RegisterFile& registers() const { return regs_; }
    const DataMemory& memory() const { return dmem_; }

private:
    struct IfId {
        bool valid = false;
        uint32_t pc = 0;
        uint32_t ir = 0;
        std::optional<Fault> fault;     // fetch fault, raised if the instruction reaches MEM
    };

    struct IdEx {
        bool valid = false;
        uint32_t pc = 0;
        DecodedInst inst;
        Control cw;
        uint32_t rs1 = 0;   // register file values, before forwarding
        uint32_t rs2 = 0;
        std::optional<Fault> fault;
    };

    struct ExMem {
        bool valid = false;
        uint32_t pc = 0;
        DecodedInst inst;
        Control cw;
        uint32_t aluOut = 0;    // memory address, or the value to write back
        uint32_t storeData = 0;
        std::optional<Fault> fault;
    };

    struct MemWb {
        bool valid = false;
        uint32_t pc = 0;
        DecodedInst inst;
        Control cw;
        uint32_t result = 0;    // value to write back
    };

    /*what happened in one cycle, for the trace*/
    struct CycleEvents {
        bool stalled = false;
        std::size_t flushed = 0;
        ForwardSource rs1 = ForwardSource::RegisterFile;
        ForwardSource rs2 = ForwardSource::RegisterFile;
    };

    /*advances every stage by one clock; returns the outcome when the program stops in this cycle*/
    std::optional<Outcome> cycle(CycleEvents& events);
    bool empty() const;
    PendingWrite pendingWrite(const ExMem& latch) const;
    PendingWrite pendingWrite(const MemWb& latch) const;
    void trace(Tracer& tracer, const CycleEvents& events) const;

    InstructionMemory imem_;
    RegisterFile regs_;
    DataMemory dmem_;
    uint32_t pc_ = 0;
    bool fetching_ = false;     // false once the PC has run past the program
    IfId ifid_;
    IdEx idex_;
    ExMem exmem_;
    MemWb memwb_;
    RunResult result_;
};

}  // namespace rv
