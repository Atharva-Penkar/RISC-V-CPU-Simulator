import assert from "node:assert/strict";
import { describe, test } from "node:test";

import { changesAt, MachineState } from "../../src/timeline/machineState.js";
import { loadTimeline, timelineFixtureNames } from "./fixtures.js";

/**
 * The state at `cycle` computed the slow, obvious way: replay every write from cycle 1.
 * @param {import("../../src/timeline/types.js").Timeline} timeline
 * @param {number} cycle
 */
function replay(timeline, cycle) {
  const registers = new Array(32).fill(0);
  const bytes = new Array(timeline.final.memory.bytes).fill(0);
  for (const record of timeline.cycles.slice(0, cycle)) {
    if (record.registerWrite) registers[record.registerWrite.register] = record.registerWrite.value;
    const access = record.memory;
    if (access?.access === "store") {
      for (let i = 0; i < access.width; i++) bytes[access.address + i] = (access.value >>> (8 * i)) & 0xff;
    }
  }
  const memory = [];
  for (let a = 0; a < bytes.length; a += 4) memory.push((bytes[a] | (bytes[a + 1] << 8) | (bytes[a + 2] << 16) | (bytes[a + 3] << 24)) >>> 0);
  return { registers, memory };
}

describe("MachineState", () => {
  test("starts at zero and ends at the simulator's final state", () => {
    for (const name of timelineFixtureNames()) {
      const timeline = loadTimeline(name);
      const state = new MachineState(timeline);
      const start = state.snapshot();
      assert.equal(start.cycle, 0);
      assert.ok(start.registers.every((v) => v === 0), name);
      assert.ok(start.memory.every((v) => v === 0), name);

      const end = state.seek(timeline.cycles.length).snapshot();
      assert.deepEqual(end.registers, timeline.final.registers, name);
      assert.deepEqual(end.memory, timeline.final.memory.words, name);
    }
  });

  test("any order of steps and jumps matches a replay from the start", () => {
    for (const name of ["loadstore", "fibonacci", "forwarding", "hazards"]) {
      const timeline = loadTimeline(name);
      const state = new MachineState(timeline);
      const last = timeline.cycles.length;
      const visits = [3, 1, last, 0, Math.floor(last / 2), Math.floor(last / 2) - 1, last - 1, 2, last, 5];
      for (const cycle of visits) {
        const snapshot = state.seek(cycle).snapshot();
        const expected = replay(timeline, cycle);
        assert.deepEqual(snapshot.registers, expected.registers, `${name} registers at ${cycle}`);
        assert.deepEqual(snapshot.memory, expected.memory, `${name} memory at ${cycle}`);
      }
    }
  });

  test("seeking clamps to the run", () => {
    const timeline = loadTimeline("forwarding");
    const state = new MachineState(timeline);
    assert.equal(state.seek(-5).cycle, 0);
    assert.equal(state.seek(10_000).cycle, timeline.cycles.length);
    assert.equal(state.seek(2.7).cycle, 2);
  });

  test("snapshots are copies", () => {
    const timeline = loadTimeline("forwarding");
    const state = new MachineState(timeline);
    const early = state.seek(1).snapshot();
    state.seek(timeline.cycles.length);
    assert.ok(early.registers.every((v) => v === 0));
  });
});

describe("changesAt", () => {
  test("reports the register and memory word written in a cycle", () => {
    const timeline = loadTimeline("loadstore");
    const writeCycle = timeline.cycles.find((c) => c.registerWrite)?.cycle ?? 0;
    assert.deepEqual([...changesAt(timeline, writeCycle).registers], [timeline.cycles[writeCycle - 1].registerWrite?.register]);
    const store = timeline.cycles.find((c) => c.memory?.access === "store");
    assert.ok(store?.memory);
    assert.deepEqual([...changesAt(timeline, store.cycle).memoryWords], [store.memory.address & ~3]);
    assert.equal(changesAt(timeline, 0).registers.size, 0);
  });
});
