// English explanations of what the pipeline did, generated from the timeline records.

import { hex, plural, registerName, signed } from "./format.js";
import { cycleEvents, instanceInfo } from "./selectors.js";

/** @typedef {import("./types.js").Timeline} Timeline */
/** @typedef {import("./selectors.js").CycleEvent} CycleEvent */

/**
 * "I3 ADD x2, x1, x1" (with "×2" for a repeated execution).
 * @param {Timeline} timeline
 * @param {number} instanceId
 */
export function describeInstance(timeline, instanceId) {
  const info = instanceInfo(timeline, instanceId);
  return `${info.tag} ${info.text}${info.badge ? ` (${info.badge})` : ""}`;
}

/**
 * A register value in both readings people look for: "5" or "-2 (0xfffffffe)".
 * @param {number} value
 */
function value(value) {
  const asSigned = signed(value);
  return asSigned >= -9 && asSigned <= 9 ? String(asSigned) : `${asSigned} (${hex(value)})`;
}

/** @type {Record<import("./types.js").FaultKind, string>} */
const FAULT_REASONS = {
  "illegal-instruction": "is not an RV32IM instruction",
  "fetch-out-of-range": "is outside the program",
  "fetch-misaligned": "is not word aligned",
  "load-out-of-range": "is outside data memory",
  "load-misaligned": "is not aligned to the size of the load",
  "store-out-of-range": "is outside data memory",
  "store-misaligned": "is not aligned to the size of the store",
};

/**
 * Why the program stopped.
 * @param {Timeline} timeline
 */
export function explainOutcome(timeline) {
  const { outcome } = timeline;
  switch (outcome.status) {
    case "finished":
      return `The program ran past its last instruction and the pipeline drained after ${plural(outcome.cycle, "cycle")}.`;
    case "halted":
      return `${describeInstance(timeline, /** @type {number} */ (outcome.instance))} reached MEM and halted the program after every older instruction had completed.`;
    case "cycle-limit":
      return `The run stopped at the cycle limit (${outcome.cycle} cycles). The program may loop forever.`;
    case "faulted": {
      const fault = /** @type {NonNullable<typeof outcome.fault>} */ (outcome.fault);
      const reason = FAULT_REASONS[fault.kind];
      const subject =
        fault.kind === "illegal-instruction"
          ? `The word ${hex(fault.address)} at ${hex(fault.pc)}`
          : fault.kind.startsWith("fetch")
            ? `The jump target ${hex(fault.address)}`
            : `The address ${hex(fault.address)}`;
      const by = outcome.instance === null ? "" : ` (${describeInstance(timeline, outcome.instance)})`;
      const memory = fault.kind.endsWith("out-of-range") && !fault.kind.startsWith("fetch")
        ? ` Data memory is ${timeline.final.memory.bytes} bytes.`
        : "";
      return `Fault${by}: ${subject} ${reason}.${memory} Older instructions completed; younger ones were discarded.`;
    }
  }
}

/**
 * One sentence for one event.
 * @param {Timeline} timeline
 * @param {CycleEvent} event
 */
export function explainEvent(timeline, event) {
  const name = (/** @type {number} */ id) => describeInstance(timeline, id);
  switch (event.kind) {
    case "register-write": {
      const { write } = event;
      const from = write.source === "memory" ? "the loaded value" : write.source === "link" ? "the return address" : "the ALU result";
      return `WB: ${name(write.instance)} writes ${from} ${value(write.value)} to ${registerName(write.register)} (was ${value(write.previous)}).`;
    }
    case "memory": {
      const { memory } = event;
      const size = plural(memory.width, "byte");
      return memory.access === "store"
        ? `MEM: ${name(memory.instance)} stores ${value(memory.value)} to ${hex(memory.address)} (${size}, was ${value(memory.previous ?? 0)}).`
        : `MEM: ${name(memory.instance)} loads ${value(memory.value)} from ${hex(memory.address)} (${size}).`;
    }
    case "forward": {
      const { forward } = event;
      const distance = forward.from === "EX/MEM" ? "the instruction just ahead" : "the instruction two ahead";
      return `Forwarding: ${registerName(forward.register)} = ${value(forward.value)} goes from ${forward.from} (${distance}, ${name(forward.producer)}) straight to ${forward.operand} of ${name(forward.consumer)} in EX, instead of the stale register file value.`;
    }
    case "branch": {
      const { execute } = event;
      return execute.branch?.taken
        ? `EX: ${name(execute.instance)} is taken; fetch continues at ${hex(/** @type {number} */ (execute.target))}.`
        : `EX: ${name(execute.instance)} is not taken; fetch continues in order.`;
    }
    case "busy": {
      const { busy } = event;
      const instance = timeline.instances[busy.instance];
      const instruction = instance.instruction === null ? null : timeline.program.instructions[instance.instruction];
      const progress = `cycle ${busy.cycle} of ${busy.of}`;
      if (busy.stage === "EX") {
        const unit = instruction?.aluOp && ["div", "divu", "rem", "remu"].includes(instruction.aluOp) ? "divider" : "multiplier";
        return busy.cycle < busy.of
          ? `EX busy: ${name(busy.instance)} is in the ${unit}, ${progress}. ID and IF hold their instructions and a bubble goes to MEM.`
          : `EX: ${name(busy.instance)} finishes in the ${unit} (${progress}); its result moves on at the end of this cycle.`;
      }
      const access = instruction?.control?.memWrite ? "store" : "load";
      return busy.cycle < busy.of
        ? `MEM busy: the ${access} ${name(busy.instance)} is waiting on memory, ${progress}. Every stage behind it holds and a bubble goes to WB.`
        : `MEM: the ${access} ${name(busy.instance)} completes its memory access (${progress}).`;
    }
    case "stall": {
      const { stall, producerStage } = event;
      if (stall.reason === "load-use") {
        return `Load-use stall: ${name(stall.waiting)} waits in ID because it needs ${registerName(stall.register)}, which the load ${name(stall.producer)} only reads from memory in MEM next cycle. A bubble enters EX and the PC holds.`;
      }
      return `Data hazard stall: ${name(stall.waiting)} waits in ID because it needs ${registerName(stall.register)}, which ${name(stall.producer)} (in ${producerStage}) has not written back yet. Forwarding is off, so the value only arrives through the register file once the producer reaches WB. A bubble enters EX and the PC holds.`;
    }
    case "flush": {
      const { flush } = event;
      const squashed = flush.squashed.length === 0
        ? "Nothing had been fetched behind it"
        : `${flush.squashed.map(name).join(" and ")} ${flush.squashed.length === 1 ? "was" : "were"} on the wrong path and ${flush.squashed.length === 1 ? "is" : "are"} flushed`;
      return `Flush: ${name(flush.by)} redirects fetch to ${hex(flush.target)}. ${squashed}; the pipeline loses two cycles.`;
    }
    case "outcome":
      return explainOutcome(timeline);
  }
}

/**
 * Every sentence for a cycle, in pipeline order.
 * @param {Timeline} timeline
 * @param {number} cycle
 * @returns {{ kind: CycleEvent["kind"], text: string }[]}
 */
export function explainCycle(timeline, cycle) {
  return cycleEvents(timeline, cycle).map((event) => ({ kind: event.kind, text: explainEvent(timeline, event) }));
}
