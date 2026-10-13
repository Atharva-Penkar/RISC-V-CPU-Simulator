import assert from "node:assert/strict";
import { describe, test } from "node:test";

import {
  cycleEvents,
  instanceInfo,
  linesInPipeline,
  scrubberMarks,
  stageOccupancy,
  STAGES,
  timingRows,
} from "../../src/timeline/selectors.js";
import { loadTimeline, timelineFixtureNames } from "./fixtures.js";

describe("stageOccupancy", () => {
  test("shows instances moving one stage per cycle", () => {
    const timeline = loadTimeline("arithmetic");
    const first = stageOccupancy(timeline, 1);
    assert.deepEqual(first.map((s) => s.stage), STAGES);
    assert.deepEqual(first[0].content, { kind: "instance", instance: 0, stalled: false, busy: null });
    assert.ok(first.slice(1).every((s) => s.content.kind === "empty"));
    const fifth = stageOccupancy(timeline, 5);
    assert.deepEqual(
      fifth.map((s) => (s.content.kind === "instance" ? s.content.instance : null)),
      [4, 3, 2, 1, 0],
    );
  });

  test("shows the held instructions and the bubble of a load-use stall", () => {
    const timeline = loadTimeline("forwarding");
    const stallCycle = /** @type {number} */ (timeline.cycles.find((c) => c.stall)?.cycle);
    const during = stageOccupancy(timeline, stallCycle);
    const id = during.find((s) => s.stage === "ID")?.content;
    assert.ok(id?.kind === "instance" && id.stalled);
    const after = stageOccupancy(timeline, stallCycle + 1);
    assert.deepEqual(after.find((s) => s.stage === "EX")?.content, { kind: "bubble", reason: "stall" });
  });

  test("shows flush bubbles after a taken branch", () => {
    const timeline = loadTimeline("branches");
    const flushCycle = /** @type {number} */ (timeline.cycles.find((c) => c.flush)?.cycle);
    const after = stageOccupancy(timeline, flushCycle + 1);
    assert.deepEqual(after.find((s) => s.stage === "ID")?.content, { kind: "bubble", reason: "flush" });
    assert.deepEqual(after.find((s) => s.stage === "EX")?.content, { kind: "bubble", reason: "flush" });
  });
});

describe("instanceInfo", () => {
  test("labels instances by static instruction, with a badge for repeats", () => {
    const timeline = loadTimeline("loop");
    const bodyInstances = timeline.instances.filter((i) => i.instruction === 2);
    assert.ok(bodyInstances.length >= 2, "the loop body runs more than once");
    const first = instanceInfo(timeline, bodyInstances[0].id);
    const second = instanceInfo(timeline, bodyInstances[1].id);
    assert.equal(first.tag, "I2");
    assert.equal(second.tag, "I2");
    assert.equal(first.badge, "");
    assert.equal(second.badge, "×2");
    assert.equal(first.text, "ADDI x1, x1, 1");
    assert.equal(first.line, timeline.program.instructions[2].source?.line);
  });

  test("describes fetches outside the program", () => {
    const timeline = loadTimeline("outcome-load-fault");
    assert.equal(instanceInfo(timeline, 0).text, "ADDI x1, x0, 256");
  });
});

describe("linesInPipeline", () => {
  test("maps source lines to the stages they occupy", () => {
    const timeline = loadTimeline("arithmetic");
    const lines = linesInPipeline(timeline, 3);
    const lineOf = (/** @type {number} */ id) => /** @type {number} */ (timeline.program.instructions[id].source?.line);
    assert.deepEqual(lines.get(lineOf(0)), ["EX"]);
    assert.deepEqual(lines.get(lineOf(1)), ["ID"]);
    assert.deepEqual(lines.get(lineOf(2)), ["IF"]);
    assert.equal(lines.size, 3);
  });

  test("lists a line twice when two executions of it are in flight", () => {
    const timeline = loadTimeline("loop");
    const multi = timeline.cycles.some((c) => [...linesInPipeline(timeline, c.cycle).values()].some((s) => s.length > 1));
    assert.ok(multi);
  });
});

describe("timingRows", () => {
  test("has a row per instance with consecutive stages", () => {
    for (const name of timelineFixtureNames()) {
      const timeline = loadTimeline(name);
      const rows = timingRows(timeline);
      assert.equal(rows.length, timeline.instances.length, name);
      for (const row of rows) {
        for (let i = 1; i < row.cells.length; i++) {
          assert.equal(row.cells[i].cycle, row.cells[i - 1].cycle + 1, `${name} instance ${row.instance} skips a cycle`);
        }
      }
      const retired = rows.find((r) => r.fate === "retired");
      if (retired) assert.deepEqual(retired.cells.map((c) => c.stage).slice(-3), ["EX", "MEM", "WB"], name);
    }
  });

  test("is computed once per timeline", () => {
    const timeline = loadTimeline("forwarding");
    assert.equal(timingRows(timeline), timingRows(timeline));
  });
});

describe("multi-cycle operations", () => {
  test("a slow multiply stays in EX and holds the stages behind it", () => {
    const timeline = loadTimeline("latency-slow");
    assert.deepEqual(timeline.latencies, { multiply: 3, divide: 4, load: 2, store: 2 });
    const mul = timingRows(timeline)[1];
    assert.deepEqual(
      mul.cells.filter((c) => c.stage === "EX").map((c) => c.busy),
      [{ cycle: 1, of: 3 }, { cycle: 2, of: 3 }, { cycle: 3, of: 3 }],
    );
    const during = stageOccupancy(timeline, 4);
    const ex = during.find((s) => s.stage === "EX")?.content;
    assert.deepEqual(ex, { kind: "instance", instance: 1, stalled: false, busy: { cycle: 1, of: 3 } });
    const id = during.find((s) => s.stage === "ID")?.content;
    assert.ok(id?.kind === "instance" && id.stalled);
    assert.deepEqual(stageOccupancy(timeline, 5).find((s) => s.stage === "MEM")?.content, { kind: "bubble", reason: "stall" });
  });

  test("busy cycles are events and marked on the scrubber", () => {
    const timeline = loadTimeline("latency-slow");
    assert.ok(cycleEvents(timeline, 4).some((e) => e.kind === "busy"));
    const holding = timeline.cycles.filter((c) => c.stall || c.busy.some((b) => b.cycle < b.of)).length;
    assert.equal(scrubberMarks(timeline).filter((m) => m.stall).length, holding);
    assert.equal(holding, timeline.stats.stalls + timeline.stats.executeStalls + timeline.stats.memoryStalls);
  });
});

describe("scrubberMarks", () => {
  test("marks every stall, forward and flush cycle", () => {
    const timeline = loadTimeline("forwarding");
    const marks = scrubberMarks(timeline);
    assert.equal(marks.filter((m) => m.stall).length, timeline.stats.stalls);
    assert.equal(marks.filter((m) => m.flush).length, timeline.stats.redirects);
    assert.ok(marks.some((m) => m.forward));
    assert.ok(marks.every((m) => m.stall || m.forward || m.flush));
  });
});

describe("cycleEvents", () => {
  test("lists a cycle's events and ends the run with its outcome", () => {
    const timeline = loadTimeline("forwarding");
    const stallCycle = /** @type {number} */ (timeline.cycles.find((c) => c.stall)?.cycle);
    assert.ok(cycleEvents(timeline, stallCycle).some((e) => e.kind === "stall"));
    const last = cycleEvents(timeline, timeline.cycles.length);
    assert.equal(last.at(-1)?.kind, "outcome");
    assert.deepEqual(cycleEvents(timeline, 0), []);
  });
});
