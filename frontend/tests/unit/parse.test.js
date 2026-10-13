import assert from "node:assert/strict";
import { describe, test } from "node:test";

import { parseProgram, parseTimeline, TimelineFormatError } from "../../src/timeline/parse.js";
import { loadFixture, timelineFixtureNames } from "./fixtures.js";

describe("parseTimeline", () => {
  test("accepts every fixture the simulator produced", () => {
    const names = timelineFixtureNames();
    assert.ok(names.length >= 10);
    for (const name of names) {
      const result = parseTimeline(loadFixture(name).timeline);
      assert.equal(result.ok, true, name);
    }
  });

  test("returns the diagnostics of a program that does not assemble", () => {
    const result = parseTimeline(loadFixture("assembly-errors").timeline);
    assert.equal(result.ok, false);
    if (result.ok) return;
    assert.deepEqual(
      result.diagnostics.map((d) => [d.line, d.column]),
      [
        [2, 1],
        [3, 1],
      ],
    );
    assert.match(result.diagnostics[0].message, /unknown instruction 'foo'/);
  });

  test("rejects other formats, versions and broken documents", () => {
    const good = loadFixture("forwarding").timeline;
    assert.throws(() => parseTimeline(null), TimelineFormatError);
    assert.throws(() => parseTimeline({ ...good, format: "rv-program" }), /Expected a "rv-timeline"/);
    assert.throws(() => parseTimeline({ ...good, version: 2 }), /Unsupported rv-timeline version 2/);
    assert.throws(() => parseTimeline({ ...good, cycles: undefined }), /cycles/);
    assert.throws(() => parseTimeline({ ...good, cycles: good.cycles.slice(1) }), /Cycle 1 is numbered 2/);
    assert.throws(() => parseTimeline({ ...good, instances: good.instances.slice(1) }), /Instance 0 has id 1/);
    assert.throws(() => parseTimeline({ ...good, ok: false }), /no diagnostics/);
  });
});

describe("parseProgram", () => {
  test("accepts an rv-program document and rejects a timeline", () => {
    const program = { format: "rv-program", version: 1, ok: true, instructions: [] };
    assert.deepEqual(parseProgram(program), { ok: true, value: program });
    assert.throws(() => parseProgram(loadFixture("forwarding").timeline), TimelineFormatError);
  });
});
