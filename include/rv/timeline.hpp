// A structured record of every pipeline cycle, for visualising a run. The pipeline fills it while it
// simulates; a viewer then reads the state at any cycle without re-running anything.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "rv/assembler.hpp"
#include "rv/cpu.hpp"

namespace rv {

enum class Stage : uint8_t { IF, ID, EX, MEM, WB };
inline constexpr std::size_t kStageCount = 5;

/*why a stage holds no instruction although the pipeline is running*/
enum class BubbleReason : uint8_t {
    Stall,  // inserted behind a stage that holds its instruction: ID in a load-use stall, a busy EX or MEM
    Flush,  // left behind by an instruction squashed after a taken branch or jump
};

/*what one stage worked on during a cycle*/
struct StageSlot {
    enum class Kind : uint8_t { Empty, Bubble, Instruction };
    Kind kind = Kind::Empty;
    uint32_t instance = 0;                      // Kind::Instruction
    BubbleReason bubble = BubbleReason::Stall;  // Kind::Bubble
    bool stalled = false;                       // held in place this cycle: a load-use stall, or a busy stage ahead
};

/*what finally happened to a dynamic instruction*/
enum class Fate : uint8_t {
    InFlight,   // still in the pipeline when the cycle limit stopped the run
    Retired,    // completed write back
    Flushed,    // squashed by a taken branch or jump
    Discarded,  // younger than an instruction that halted or faulted
    Faulted,
    Halted,     // the ECALL/EBREAK that stopped the run
};

/*one fetch of an instruction. A loop body fetched three times is three instances, so the same
source line can sit in two stages at once with different instance ids*/
struct InstanceRecord {
    uint32_t id = 0;
    uint32_t pc = 0;
    std::optional<uint32_t> instruction;    // static index (pc / 4); nullopt for a fetch outside the program
    uint32_t occurrence = 0;                // 1 for the first fetch of this static instruction, 2 for the second...
    std::size_t fetchCycle = 0;
    Fate fate = Fate::InFlight;
    std::size_t endCycle = 0;               // cycle in which it retired, was squashed, faulted or halted
};

enum class Operand : uint8_t { Rs1, Rs2 };

/*a value forwarded into EX instead of the stale register file value read in ID*/
struct ForwardRecord {
    Operand operand = Operand::Rs1;
    ForwardSource from = ForwardSource::ExMem;
    Reg reg;
    uint32_t value = 0;
    uint32_t producer = 0;  // instance that computed the value
    uint32_t consumer = 0;  // instance in EX that used it
};

/*why ID held an instruction for a cycle*/
enum class StallReason : uint8_t {
    LoadUse,        // the load in EX has not read memory yet; forwarding supplies the value one cycle later
    NoForwarding,   // forwarding is off: the producer in EX or MEM has not written the register back yet
};

/*ID held an instruction for a cycle because a value it needs is not available yet*/
struct StallRecord {
    StallReason reason = StallReason::LoadUse;
    uint32_t waiting = 0;   // instance held in ID
    uint32_t producer = 0;  // the older instance (in EX or MEM) that writes the register
    Reg reg;                // register the producer writes and the waiting instruction reads
};

/*an instruction spending more than one cycle in EX (multiply, divide) or MEM (load, store)*/
struct BusyRecord {
    Stage stage = Stage::EX;
    uint32_t instance = 0;
    unsigned cycle = 1;     // which of its cycles in the stage this is, 1-based
    unsigned latency = 1;   // cycles it needs in the stage
};

/*a taken branch or jump in EX squashed the instructions fetched behind it*/
struct FlushRecord {
    uint32_t by = 0;                    // the branch or jump instance
    std::vector<uint32_t> squashed;     // instances discarded from IF/ID and ID/EX (may be empty)
    uint32_t target = 0;
};

/*the EX stage datapath for the instruction executing this cycle*/
struct ExecuteRecord {
    uint32_t instance = 0;
    AluOp op = AluOp::Add;
    uint32_t a = 0;                     // ALU input A after the forwarding and PC muxes
    uint32_t b = 0;                     // ALU input B after the forwarding and ALUSrc muxes
    uint32_t result = 0;                // ALU output (for branches: 1 when the condition holds)
    bool aFromPc = false;               // AUIPC, JAL: A is the PC instead of rs1
    bool bFromImmediate = false;        // ALUSrc
    std::optional<bool> branchTaken;    // set for conditional branches
    std::optional<uint32_t> target;     // next PC when a branch is taken or for a jump
};

/*a data memory access in MEM*/
struct MemoryRecord {
    uint32_t instance = 0;
    bool store = false;
    uint32_t address = 0;
    uint32_t width = 0;     // bytes
    uint32_t value = 0;     // stored value (low width bytes), or loaded value after sign/zero extension
    uint32_t previous = 0;  // stores: the bytes that were overwritten
};

/*where the value written back comes from (the MemToReg mux, plus the link address of jumps)*/
enum class WriteSource : uint8_t { Alu, Memory, Link };

struct RegisterWriteRecord {
    uint32_t instance = 0;
    Reg reg;
    uint32_t value = 0;
    uint32_t previous = 0;
    WriteSource source = WriteSource::Alu;
};

/*the PC mux at the end of the cycle*/
enum class NextPcSource : uint8_t {
    Sequential,     // PC + 4
    BranchTarget,   // a taken branch in EX
    JumpTarget,     // JAL/JALR in EX
    Hold,           // a stall (load-use, or a busy EX or MEM): PC is not written
    Stopped,        // fetching has stopped (end of program)
};

struct CycleRecord {
    std::size_t cycle = 0;                          // 1-based
    uint32_t pc = 0;                                // fetch address at the start of the cycle
    std::array<StageSlot, kStageCount> stages{};    // indexed by Stage
    std::vector<ForwardRecord> forwards;
    std::optional<StallRecord> stall;
    std::vector<BusyRecord> busy;                   // multi-cycle operations in EX and MEM, in pipeline order
    std::optional<FlushRecord> flush;
    std::optional<ExecuteRecord> execute;
    std::optional<MemoryRecord> memory;
    std::optional<RegisterWriteRecord> registerWrite;
    NextPcSource nextPcSource = NextPcSource::Stopped;
    uint32_t nextPc = 0;
};

/*a whole pipeline run: the program with its source map, every dynamic instruction, every cycle,
the outcome and the final machine state*/
class Timeline {
public:
    const Program& words() const { return words_; }
    /*empty when the program came without a source map (machine code input)*/
    const std::vector<SourceLocation>& sources() const { return sources_; }
    const std::vector<std::optional<DecodedInst>>& decoded() const { return decoded_; }

    const std::vector<InstanceRecord>& instances() const { return instances_; }
    const std::vector<CycleRecord>& cycles() const { return cycles_; }
    std::size_t cycleCount() const { return cycles_.size(); }
    /*cycle is 1-based*/
    const CycleRecord& at(std::size_t cycle) const { return cycles_.at(cycle - 1); }

    const RunResult& result() const { return result_; }
    const PipelineConfig& config() const { return config_; }
    /*the instance that faulted or halted, when the outcome is Faulted or Halted*/
    std::optional<uint32_t> stopInstance() const { return stopInstance_; }

    /*register values at the end of a cycle (0: before the first cycle), replayed from the writes*/
    std::array<uint32_t, 32> registersAfter(std::size_t cycle) const;
    /*data memory at the end of a cycle, replayed from the stores*/
    DataMemory memoryAfter(std::size_t cycle) const;

    const RegisterFile& finalRegisters() const { return finalRegisters_; }
    const DataMemory& finalMemory() const { return finalMemory_; }

private:
    friend class Pipeline;
    friend Timeline recordPipeline(AssembledProgram program, std::size_t maxCycles, PipelineConfig config);

    Program words_;
    std::vector<SourceLocation> sources_;
    std::vector<std::optional<DecodedInst>> decoded_;
    std::vector<InstanceRecord> instances_;
    std::vector<uint32_t> occurrences_;     // fetches so far per static instruction
    std::vector<CycleRecord> cycles_;
    RunResult result_;
    PipelineConfig config_;
    std::optional<uint32_t> stopInstance_;
    RegisterFile finalRegisters_;
    DataMemory finalMemory_;
};

/*runs the program on the pipeline and records every cycle*/
Timeline recordPipeline(AssembledProgram program, std::size_t maxCycles = kMaxCycles, PipelineConfig config = {});

}  // namespace rv
