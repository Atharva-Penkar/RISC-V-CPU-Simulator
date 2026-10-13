import assert from "node:assert/strict";
import { describe, test } from "node:test";

import { instanceColor, instructionColor, occurrenceBadge, PALETTE_SIZE } from "../../src/timeline/colors.js";
import { abiName, formatValue, hex, plural, registerName, signed, unsigned } from "../../src/timeline/format.js";
import { loadTimeline } from "./fixtures.js";

describe("format", () => {
  test("hex, signed and unsigned readings of 32-bit values", () => {
    assert.equal(hex(10), "0x0000000a");
    assert.equal(hex(0xfffffffe), "0xfffffffe");
    assert.equal(hex(8, 3), "0x008");
    assert.equal(signed(0xfffffffe), -2);
    assert.equal(signed(5), 5);
    assert.equal(unsigned(-2), 0xfffffffe);
    assert.equal(formatValue(0xfffffffe, "hex"), "0xfffffffe");
    assert.equal(formatValue(0xfffffffe, "signed"), "-2");
    assert.equal(formatValue(0xfffffffe, "unsigned"), "4294967294");
  });

  test("register names and plurals", () => {
    assert.equal(registerName(5), "x5");
    assert.equal(abiName(0), "zero");
    assert.equal(abiName(5), "t0");
    assert.equal(abiName(31), "t6");
    assert.equal(plural(1, "byte"), "1 byte");
    assert.equal(plural(4, "byte"), "4 bytes");
  });
});

describe("colors", () => {
  test("every execution of an instruction shares its colour", () => {
    assert.equal(instructionColor(3), "var(--inst-3)");
    assert.equal(instructionColor(PALETTE_SIZE + 1), "var(--inst-1)");
    assert.equal(instructionColor(null), "var(--inst-none)");
    const timeline = loadTimeline("loop");
    const body = timeline.instances.filter((i) => i.instruction === 2);
    assert.equal(instanceColor(timeline, body[0].id), instanceColor(timeline, body[1].id));
  });

  test("occurrence badges", () => {
    assert.equal(occurrenceBadge(1), "");
    assert.equal(occurrenceBadge(3), "×3");
  });
});
