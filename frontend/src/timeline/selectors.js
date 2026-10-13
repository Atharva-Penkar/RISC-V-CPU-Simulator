// Read-only views of a timeline for the UI. Pure functions of (timeline, cycle); anything that scans
// the whole run is computed once per timeline and cached.

import { occurrenceBadge } from "./colors.js";
import { hex } from "./format.js";

/** @typedef {import("./types.js").Timeline} Timeline */
/** @typedef {import("./types.js").StageName} StageName */
/** @typedef {import("./types.js").Instruction} Instruction */

/** @type {StageName[]} */
export const STAGES = ["IF", "ID", "EX", "MEM", "WB"];

/**
 * Caches a whole-run computation per timeline object.
 * @template T
 * @param {(timeline: Timeline) => T} compute
 * @returns {(timeline: Timeline) => T}
 */
function perTimeline(compute) {
  /** @type {WeakMap<Timeline, T>} */
  const cache = new WeakMap();
  return (timeline) => {
    if (!cache.has(timeline)) cache.set(timeline, compute(timeline));
    return /** @type {T} */ (cache.get(timeline));
  };
}

/**
 * @typedef {object} InstanceInfo
 * @property {number} id
 * @property {Instruction | null} instruction  null for a fetch outside the program
 * @property {string} tag      "I3": the static instruction, the same for every execution of a line
 * @property {string} text     disassembly, or a description of an invalid fetch
 * @property {string} badge    "" for the first execution, "×2" for the second...
 * @property {number | null} line  source line, if known
 */

/**
 * Everything needed to label an instance (InstructionChip, timing chart, event log).
 * @param {Timeline} timeline
 * @param {number} instanceId
 * @returns {InstanceInfo}
 */
export function instanceInfo(timeline, instanceId) {
  const instance = timeline.instances[instanceId];
  const instruction = instance.instruction === null ? null : timeline.program.instructions[instance.instruction];
  let text;
  if (!instruction) text = `fetch at ${hex(instance.pc)}`;
  else if (!instruction.valid) text = `invalid word ${instruction.word}`;
  else text = /** @type {string} */ (instruction.text);
  return {
    id: instanceId,
    instruction,
    tag: instruction ? `I${instruction.id}` : "I?",
    text,
    badge: occurrenceBadge(instance.occurrence),
    line: instruction?.source?.line ?? null,
  };
}

/**
 * @typedef {{ kind: "empty" } | { kind: "bubble", reason: "stall" | "flush" }
 *   | { kind: "instance", instance: number, stalled: boolean }} StageContent
 */

/**
 * What each stage works on during a cycle, in pipeline order.
 * @param {Timeline} timeline
 * @param {number} cycle  1-based
 * @returns {{ stage: StageName, content: StageContent }[]}
 */
export function stageOccupancy(timeline, cycle) {
  const record = timeline.cycles[cycle - 1];
  return STAGES.map((stage) => {
    const slot = record?.stages[stage] ?? null;
    /** @type {StageContent} */
    let content = { kind: "empty" };
    if (slot && "bubble" in slot) content = { kind: "bubble", reason: slot.bubble };
    else if (slot) content = { kind: "instance", instance: slot.instance, stalled: slot.stalled === true };
    return { stage, content };
  });
}

/**
 * Source lines that are in the pipeline during a cycle, with their stages (a line in a loop can be in
 * two stages at once). Drives the editor's stage gutter.
 * @param {Timeline} timeline
 * @param {number} cycle
 * @returns {Map<number, StageName[]>}
 */
export function linesInPipeline(timeline, cycle) {
  /** @type {Map<number, StageName[]>} */
  const lines = new Map();
  for (const { stage, content } of stageOccupancy(timeline, cycle)) {
    if (content.kind !== "instance") continue;
    const line = instanceInfo(timeline, content.instance).line;
    if (line === null) continue;
    lines.set(line, [...(lines.get(line) ?? []), stage]);
  }
  return lines;
}

/**
 * @typedef {object} TimingCell
 * @property {number} cycle
 * @property {StageName} stage
 * @property {boolean} stalled
 */

/**
 * @typedef {object} TimingRow
 * @property {number} instance
 * @property {TimingCell[]} cells  in cycle order
 * @property {import("./types.js").Fate} fate
 * @property {number} endCycle
 */

/**
 * One row per instance (in fetch order) with the stage it occupied in each cycle: the classic
 * pipeline diagram, rows = instructions, columns = cycles.
 * @type {(timeline: Timeline) => TimingRow[]}
 */
export const timingRows = perTimeline((timeline) => {
  /** @type {TimingRow[]} */
  const rows = timeline.instances.map((instance) => ({
    instance: instance.id,
    cells: [],
    fate: instance.fate,
    endCycle: instance.endCycle,
  }));
  for (const record of timeline.cycles) {
    for (const stage of STAGES) {
      const slot = record.stages[stage];
      if (slot && "instance" in slot) {
        rows[slot.instance].cells.push({ cycle: record.cycle, stage, stalled: slot.stalled === true });
      }
    }
  }
  return rows;
});

/**
 * @typedef {object} ScrubberMark
 * @property {number} cycle
 * @property {boolean} stall
 * @property {boolean} forward
 * @property {boolean} flush
 */

/**
 * Cycles with a stall, a forwarded operand or a flush, for the ticks on the scrubber.
 * @type {(timeline: Timeline) => ScrubberMark[]}
 */
export const scrubberMarks = perTimeline((timeline) =>
  timeline.cycles
    .map((record) => ({
      cycle: record.cycle,
      stall: record.stall !== null,
      forward: record.forwarding.length > 0,
      flush: record.flush !== null,
    }))
    .filter((mark) => mark.stall || mark.forward || mark.flush),
);

/**
 * @typedef {{ kind: "forward", forward: import("./types.js").Forward }
 *   | { kind: "stall", stall: import("./types.js").Stall }
 *   | { kind: "flush", flush: import("./types.js").Flush }
 *   | { kind: "branch", execute: import("./types.js").Execute }
 *   | { kind: "memory", memory: import("./types.js").MemoryAccess }
 *   | { kind: "register-write", write: import("./types.js").RegisterWrite }
 *   | { kind: "outcome", outcome: import("./types.js").Outcome }} CycleEvent
 */

/**
 * The notable things that happened during a cycle, in pipeline order (WB first, as the hardware does
 * the register write in the first half of the cycle). explain.js turns them into sentences.
 * @param {Timeline} timeline
 * @param {number} cycle
 * @returns {CycleEvent[]}
 */
export function cycleEvents(timeline, cycle) {
  const record = timeline.cycles[cycle - 1];
  if (!record) return [];
  /** @type {CycleEvent[]} */
  const events = [];
  if (record.registerWrite) events.push({ kind: "register-write", write: record.registerWrite });
  if (record.memory) events.push({ kind: "memory", memory: record.memory });
  for (const forward of record.forwarding) events.push({ kind: "forward", forward });
  if (record.execute?.branch) events.push({ kind: "branch", execute: record.execute });
  if (record.stall) events.push({ kind: "stall", stall: record.stall });
  if (record.flush) events.push({ kind: "flush", flush: record.flush });
  if (cycle === timeline.cycles.length) events.push({ kind: "outcome", outcome: timeline.outcome });
  return events;
}
