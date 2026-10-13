import assert from "node:assert/strict";
import { describe, test } from "node:test";

import { describeInstance, explainCycle, explainOutcome } from "../../src/timeline/explain.js";
import { loadTimeline, timelineFixtureNames } from "./fixtures.js";

/**
 * Every sentence of a run, joined.
 * @param {import("../../src/timeline/types.js").Timeline} timeline
 */
function allText(timeline) {
  return timeline.cycles.flatMap((c) => explainCycle(timeline, c.cycle).map((e) => e.text)).join("\n");
}

describe("explainCycle", () => {
  test("explains forwarding, load-use stalls and flushes", () => {
    const text = allText(loadTimeline("forwarding"));
    assert.match(text, /Forwarding: x1 = 10 \(0x0000000a\) goes from EX\/MEM \(the instruction just ahead, I0 ADDI x1, x0, 10\) straight to rs1 of I1 ADDI x2, x1, 5/);
    assert.match(text, /MEM\/WB \(the instruction two ahead/);
    assert.match(text, /Load-use stall: I5 ADDI x5, x4, 1 waits in ID because it needs x4, which the load I4 LW x4, 0\[x0\]/);
    assert.match(text, /Flush: I9 JALR x8, x7, 0 redirects fetch to 0x0000002c\. I10 ADDI x9, x0, 1 and I11 ADDI x10, x8, 0 were on the wrong path/);
  });

  test("explains register writes, loads, stores and branches", () => {
    const loads = allText(loadTimeline("loadstore"));
    assert.match(loads, /WB: I0 ADDI x1, x0, 8 writes the ALU result 8 to x1 \(was 0\)\./);
    assert.match(loads, /MEM: I2 SW x4, 0\[x1\] stores -2 to 0x00000008 \(4 bytes, was 0\)\./);
    assert.match(loads, /MEM: I4 LBU x6, 0\[x1\] loads 254 \(0x000000fe\) from 0x00000008 \(1 byte\)\./);
    assert.match(loads, /writes the loaded value/);
    const branches = allText(loadTimeline("branches"));
    assert.match(branches, /EX: I2 BLT x1, x2, pc\+8 is taken; fetch continues at 0x00000010\./);
    assert.match(branches, /is not taken; fetch continues in order\./);
    assert.match(allText(loadTimeline("jumps")), /writes the return address 8 to x1/);
  });

  test("explains multi-cycle multiplies, divides, loads and stores", () => {
    const text = allText(loadTimeline("latency-slow"));
    assert.match(text, /EX busy: I1 MUL x2, x1, x1 is in the multiplier, cycle 1 of 3\. ID and IF hold their instructions/);
    assert.match(text, /EX: I1 MUL x2, x1, x1 finishes in the multiplier \(cycle 3 of 3\)/);
    assert.match(text, /EX busy: I6 DIV x6, x5, x1 is in the divider, cycle 2 of 4/);
    assert.match(text, /MEM busy: the store I3 SW x3, 0\[x0\] is waiting on memory, cycle 1 of 2\. Every stage behind it holds/);
    assert.match(text, /MEM: the load I4 LW x4, 0\[x0\] completes its memory access \(cycle 2 of 2\)/);
  });

  test("marks repeated executions", () => {
    const timeline = loadTimeline("loop");
    const repeat = timeline.instances.find((i) => i.occurrence === 2);
    assert.ok(repeat);
    assert.match(describeInstance(timeline, repeat.id), /\(×2\)$/);
  });

  test("produces a sentence for every event of every fixture", () => {
    for (const name of timelineFixtureNames()) {
      const timeline = loadTimeline(name);
      for (const cycle of timeline.cycles) {
        for (const event of explainCycle(timeline, cycle.cycle)) {
          assert.ok(event.text.length > 10 && !event.text.includes("undefined"), `${name} cycle ${cycle.cycle}: ${event.text}`);
        }
      }
    }
  });
});

describe("explainOutcome", () => {
  test("explains each way a run can end", () => {
    assert.match(explainOutcome(loadTimeline("arithmetic")), /ran past its last instruction and the pipeline drained after 25 cycles/);
    assert.match(explainOutcome(loadTimeline("outcome-halt")), /I2 EBREAK reached MEM and halted the program/);
    assert.match(explainOutcome(loadTimeline("outcome-cycle-limit")), /cycle limit \(120 cycles\)/);
    assert.equal(
      explainOutcome(loadTimeline("outcome-load-fault")),
      "Fault (I1 LW x2, 0[x1]): The address 0x00000100 is outside data memory. Data memory is 256 bytes. " +
        "Older instructions completed; younger ones were discarded.",
    );
    assert.match(explainOutcome(loadTimeline("outcome-misaligned-store")), /The address 0x00000003 is not aligned to the size of the store\./);
  });
});
