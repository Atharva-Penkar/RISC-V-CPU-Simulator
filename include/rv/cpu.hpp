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

class Timeline;
enum class BubbleReason : uint8_t;
enum class StallReason : uint8_t;

inline constexpr std::size_t kMaxCycles = 10000;    // guards against programs that never terminate

/*how many cycles the slow operations spend in their stage. 1 everywhere is the classic 5-stage pipeline;
larger values model an iterative multiplier or divider in EX, or a slower memory in MEM. A stage that is
still busy holds itself and every stage behind it, and sends a bubble on*/
struct Latencies {
    unsigned multiply = 1;  // MUL, MULH, MULHSU, MULHU in EX
    unsigned divide = 1;    // DIV, DIVU, REM, REMU in EX
    unsigned load = 1;      // loads in MEM
    unsigned store = 1;     // stores in MEM

    static constexpr unsigned kMax = 100;
    bool valid() const {
        auto ok = [](unsigned value) { return value >= 1 && value <= kMax; };
        return ok(multiply) && ok(divide) && ok(load) && ok(store);
    }
    bool operator==(const Latencies&) const = default;
};

/*how the pipeline is built: the latencies of the slow operations, and whether results are forwarded into
EX. Without forwarding, an instruction waits in ID until every older instruction it reads from has reached
WB (the register file is written in the first half of the cycle and read in the second)*/
struct PipelineConfig {
    Latencies latencies;
    bool forwarding = true;
};

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
    uint32_t aluA = 0;                  // ALU inputs and output, for the timeline
    uint32_t aluB = 0;
    uint32_t aluOut = 0;
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

    /*the instruction in ID reads a register that write will change; x0 never counts*/
    static bool dependsOn(const DecodedInst& inId, const Control& idControl, const PendingWrite& write);

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
    std::size_t stalls = 0;         // cycles a data hazard held ID: load-use, or any dependency without forwarding
    std::size_t executeStalls = 0;  // cycles a multi-cycle multiply or divide held EX and the stages behind it
    std::size_t memoryStalls = 0;   // cycles a multi-cycle load or store held MEM and the stages behind it
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
instruction completes and no younger one has changed any state. Multiply, divide, load and store can
take several cycles (Latencies); the busy stage holds the stages behind it. With forwarding turned off,
dependent instructions wait in ID until the value is written back*/
class Pipeline {
public:
    explicit Pipeline(InstructionMemory program, PipelineConfig config = {});

    RunResult run(Tracer& tracer, std::size_t maxCycles = kMaxCycles);

    /*record every cycle into timeline during run(); timeline must outlive the run*/
    void record(Timeline& timeline) { timeline_ = &timeline; }

    const RegisterFile& registers() const { return regs_; }
    const DataMemory& memory() const { return dmem_; }

private:
    /*every pipeline register also carries the dynamic instance id of its instruction, and, when it
    holds no instruction, whether it is a bubble (only used by the timeline)*/
    struct IfId {
        bool valid = false;
        uint32_t pc = 0;
        uint32_t ir = 0;
        std::optional<Fault> fault;     // fetch fault, raised if the instruction reaches MEM
        uint32_t instance = 0;
        std::optional<BubbleReason> bubble;
    };

    struct IdEx {
        bool valid = false;
        uint32_t pc = 0;
        DecodedInst inst;
        Control cw;
        uint32_t rs1 = 0;   // register file values, before forwarding
        uint32_t rs2 = 0;
        std::optional<Fault> fault;
        uint32_t instance = 0;
        std::optional<BubbleReason> bubble;
    };

    struct ExMem {
        bool valid = false;
        uint32_t pc = 0;
        DecodedInst inst;
        Control cw;
        uint32_t aluOut = 0;    // memory address, or the value to write back
        uint32_t storeData = 0;
        std::optional<Fault> fault;
        uint32_t instance = 0;
        std::optional<BubbleReason> bubble;
    };

    struct MemWb {
        bool valid = false;
        uint32_t pc = 0;
        DecodedInst inst;
        Control cw;
        uint32_t result = 0;    // value to write back
        uint32_t instance = 0;
        std::optional<BubbleReason> bubble;
    };

    /*what happened in one cycle, for the trace*/
    struct CycleEvents {
        bool stalled = false;
        StallReason stallReason{};
        bool executeBusy = false;
        bool memoryBusy = false;
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
    /*a new dynamic instance for a fetch at pc*/
    uint32_t newInstance(uint32_t pc);
    /*cycles the instruction in a latch needs in EX or MEM*/
    unsigned executeLatency(const IdEx& latch) const;
    unsigned memoryLatency(const ExMem& latch) const;

    InstructionMemory imem_;
    RegisterFile regs_;
    DataMemory dmem_;
    PipelineConfig config_;
    uint32_t pc_ = 0;
    bool fetching_ = false;     // false once the PC has run past the program
    unsigned executeCycles_ = 0;    // cycles the instruction in ID/EX has already spent in EX
    unsigned memoryCycles_ = 0;     // cycles the instruction in EX/MEM has already spent in MEM
    IfId ifid_;
    IdEx idex_;
    ExMem exmem_;
    MemWb memwb_;
    RunResult result_;
    uint32_t nextInstance_ = 0;
    std::optional<uint32_t> pendingFetch_;  // instance shown in IF while a stall holds the PC
    Timeline* timeline_ = nullptr;
};

}  // namespace rv
