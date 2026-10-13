// JSDoc types for the simulator's JSON documents (docs/timeline-format.md, version 2).
// Values are unsigned 32-bit numbers; cycles are 1-based; instruction and instance ids are 0-based.

/** @typedef {"IF" | "ID" | "EX" | "MEM" | "WB"} StageName */

/**
 * @typedef {object} SourceLocation
 * @property {number} line    1-based
 * @property {number} column  1-based
 */

/**
 * @typedef {object} ControlSignals
 * @property {boolean} regRead1
 * @property {boolean} regRead2
 * @property {boolean} aluSrc
 * @property {boolean} regWrite
 * @property {boolean} memRead
 * @property {boolean} memWrite
 * @property {boolean} memToReg
 * @property {boolean} branch
 * @property {boolean} jump
 * @property {boolean} halt
 */

/**
 * A static instruction: one word of the program.
 * @typedef {object} Instruction
 * @property {number} id
 * @property {number} pc
 * @property {string} word          hex, e.g. "0x00500093"
 * @property {SourceLocation | null} source
 * @property {boolean} valid        false for a word that is not an RV32I instruction; the fields below are then absent
 * @property {string} [text]        disassembly, e.g. "ADDI x1, x0, 5"
 * @property {string} [mnemonic]
 * @property {string} [kind]
 * @property {string} [format]
 * @property {number | null} [rd]
 * @property {number | null} [rs1]
 * @property {number | null} [rs2]
 * @property {number | null} [imm]
 * @property {string} [aluOp]
 * @property {ControlSignals} [control]
 */

/** @typedef {"in-flight" | "retired" | "flushed" | "discarded" | "faulted" | "halted"} Fate */

/**
 * A dynamic instruction: one fetch. A loop body fetched three times is three instances.
 * @typedef {object} Instance
 * @property {number} id
 * @property {number | null} instruction  static instruction id; null for a fetch outside the program
 * @property {number} pc
 * @property {number} occurrence          1 for the first fetch of the instruction, 2 for the second...
 * @property {number} fetchCycle
 * @property {Fate} fate
 * @property {number} endCycle            0 while in flight
 */

/**
 * What one stage worked on during a cycle: null (nothing), a bubble, or an instance.
 * @typedef {null | { bubble: "stall" | "flush" } | { instance: number, stalled?: boolean }} Slot
 */

/**
 * @typedef {object} Forward
 * @property {"rs1" | "rs2"} operand
 * @property {"EX/MEM" | "MEM/WB"} from
 * @property {number} register
 * @property {number} value
 * @property {number} producer  instance that computed the value
 * @property {number} consumer  instance in EX that used it
 */

/**
 * @typedef {object} Stall
 * @property {number} waiting   instance held in ID
 * @property {number} load      the load in EX
 * @property {number} register
 */

/**
 * An instruction spending more than one cycle in EX (multiply, divide) or MEM (load, store).
 * @typedef {object} Busy
 * @property {"EX" | "MEM"} stage
 * @property {number} instance
 * @property {number} cycle     which of its cycles in the stage this is, 1-based
 * @property {number} of        cycles it needs in the stage
 */

/**
 * Cycles the slow operations take: multiply and divide in EX, load and store in MEM.
 * @typedef {object} Latencies
 * @property {number} multiply
 * @property {number} divide
 * @property {number} load
 * @property {number} store
 */

/**
 * @typedef {object} Flush
 * @property {number} by          the branch or jump instance
 * @property {number[]} squashed
 * @property {number} target
 */

/**
 * @typedef {object} Execute
 * @property {number} instance
 * @property {string} op
 * @property {number} a
 * @property {"rs1" | "pc"} aSource
 * @property {number} b
 * @property {"rs2" | "imm"} bSource
 * @property {number} result
 * @property {{ taken: boolean } | null} branch
 * @property {number | null} target
 */

/**
 * @typedef {object} MemoryAccess
 * @property {number} instance
 * @property {"load" | "store"} access
 * @property {number} address
 * @property {number} width      bytes: 1, 2 or 4
 * @property {number} value
 * @property {number} [previous] stores only: the bytes that were overwritten
 */

/**
 * @typedef {object} RegisterWrite
 * @property {number} instance
 * @property {number} register
 * @property {number} value
 * @property {number} previous
 * @property {"alu" | "memory" | "link"} source
 */

/** @typedef {"pc+4" | "branch-target" | "jump-target" | "hold" | "stopped"} NextPcSource */

/**
 * @typedef {object} Cycle
 * @property {number} cycle
 * @property {number} pc
 * @property {Record<StageName, Slot>} stages
 * @property {Forward[]} forwarding
 * @property {Stall | null} stall
 * @property {Busy[]} busy
 * @property {Flush | null} flush
 * @property {Execute | null} execute
 * @property {MemoryAccess | null} memory
 * @property {RegisterWrite | null} registerWrite
 * @property {{ source: NextPcSource, value: number }} nextPc
 */

/**
 * @typedef {"illegal-instruction" | "fetch-out-of-range" | "fetch-misaligned" | "load-out-of-range"
 *   | "load-misaligned" | "store-out-of-range" | "store-misaligned"} FaultKind
 */

/**
 * @typedef {object} Outcome
 * @property {"finished" | "halted" | "faulted" | "cycle-limit"} status
 * @property {number} cycle
 * @property {number | null} instance
 * @property {number | null} haltPc
 * @property {{ kind: FaultKind, pc: number, address: number } | null} fault
 */

/**
 * @typedef {object} Stats
 * @property {number} cycles
 * @property {number} instructions
 * @property {number} cpi
 * @property {number} stalls         load-use stall cycles
 * @property {number} executeStalls  cycles a busy EX (multiply, divide) held the pipeline
 * @property {number} memoryStalls   cycles a busy MEM (load, store) held the pipeline
 * @property {number} flushes
 * @property {number} redirects
 */

/**
 * A whole pipeline run ("rv-timeline" document with ok: true).
 * @typedef {object} Timeline
 * @property {"rv-timeline"} format
 * @property {1} version
 * @property {true} ok
 * @property {"pipeline"} model
 * @property {Latencies} latencies
 * @property {{ instructions: Instruction[] }} program
 * @property {Instance[]} instances  instances[i].id === i
 * @property {Cycle[]} cycles        cycles[i].cycle === i + 1
 * @property {Outcome} outcome
 * @property {Stats} stats
 * @property {{ registers: number[], memory: { bytes: number, words: number[] } }} final
 */

/**
 * An assembled program ("rv-program" document with ok: true).
 * @typedef {object} Program
 * @property {"rv-program"} format
 * @property {1} version
 * @property {true} ok
 * @property {Instruction[]} instructions
 */

/**
 * @typedef {object} Diagnostic
 * @property {number} line
 * @property {number} column
 * @property {string} message
 */

export {};
