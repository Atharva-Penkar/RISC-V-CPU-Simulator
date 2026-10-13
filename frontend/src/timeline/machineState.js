// Registers and data memory at any cycle. Cycle 0 is the state before the first cycle (all zero); the state
// at cycle N has the register and memory writes of cycles 1..N applied. Moving forward applies a cycle's
// writes, moving backward undoes them with the recorded `previous` values, so stepping or scrubbing costs
// only the cycles in between.

/** @typedef {import("./types.js").Timeline} Timeline */
/** @typedef {import("./types.js").Cycle} Cycle */

/**
 * Writes `width` bytes of `value` at `address`, little-endian.
 * @param {Uint8Array} bytes
 * @param {number} address
 * @param {number} width
 * @param {number} value
 */
function writeBytes(bytes, address, width, value) {
  for (let i = 0; i < width; i++) bytes[address + i] = (value >>> (8 * i)) & 0xff;
}

/**
 * @param {Uint8Array} bytes
 * @param {number} address
 */
function readWord(bytes, address) {
  return (bytes[address] | (bytes[address + 1] << 8) | (bytes[address + 2] << 16) | (bytes[address + 3] << 24)) >>> 0;
}

/**
 * @typedef {object} MachineSnapshot
 * @property {number} cycle
 * @property {number[]} registers  32 values, x0..x31
 * @property {number[]} memory     data memory as 32-bit little-endian words
 */

export class MachineState {
  /** @param {Timeline} timeline */
  constructor(timeline) {
    this.timeline = timeline;
    this.cycle = 0;
    this.registers = new Uint32Array(32);
    this.memory = new Uint8Array(timeline.final.memory.bytes);
  }

  get cycleCount() {
    return this.timeline.cycles.length;
  }

  /**
   * Moves to the end of `cycle` (clamped to 0..cycleCount).
   * @param {number} cycle
   */
  seek(cycle) {
    const target = Math.max(0, Math.min(this.cycleCount, Math.trunc(cycle)));
    while (this.cycle < target) this.#apply(this.timeline.cycles[this.cycle++]);
    while (this.cycle > target) this.#undo(this.timeline.cycles[--this.cycle]);
    return this;
  }

  /** A copy of the current state, safe to keep while the cursor moves on. */
  snapshot() {
    /** @type {number[]} */
    const memory = [];
    for (let address = 0; address + 4 <= this.memory.length; address += 4) memory.push(readWord(this.memory, address));
    return /** @type {MachineSnapshot} */ ({ cycle: this.cycle, registers: Array.from(this.registers), memory });
  }

  /** @param {Cycle} cycle */
  #apply(cycle) {
    if (cycle.registerWrite) this.registers[cycle.registerWrite.register] = cycle.registerWrite.value;
    const access = cycle.memory;
    if (access?.access === "store") writeBytes(this.memory, access.address, access.width, access.value);
  }

  /** @param {Cycle} cycle */
  #undo(cycle) {
    if (cycle.registerWrite) this.registers[cycle.registerWrite.register] = cycle.registerWrite.previous;
    const access = cycle.memory;
    if (access?.access === "store") writeBytes(this.memory, access.address, access.width, access.previous ?? 0);
  }
}

/**
 * What changed during a cycle, for "changed this cycle" highlighting.
 * @param {Timeline} timeline
 * @param {number} cycle  1-based; 0 changes nothing
 * @returns {{ registers: Set<number>, memoryWords: Set<number> }}  register numbers and word-aligned addresses
 */
export function changesAt(timeline, cycle) {
  const registers = new Set();
  const memoryWords = new Set();
  const record = timeline.cycles[cycle - 1];
  if (record?.registerWrite) registers.add(record.registerWrite.register);
  if (record?.memory?.access === "store") memoryWords.add(record.memory.address & ~3);
  return { registers, memoryWords };
}
