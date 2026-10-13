// Number and register formatting. Values in the timeline are unsigned 32-bit numbers.

const ABI_NAMES = [
  "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
];

/**
 * "0x0000000a"; digits pads with zeros.
 * @param {number} value
 * @param {number} [digits]
 */
export function hex(value, digits = 8) {
  return "0x" + (value >>> 0).toString(16).padStart(digits, "0");
}

/**
 * The value read as a signed 32-bit integer: 0xfffffffe -> -2.
 * @param {number} value
 */
export function signed(value) {
  return value | 0;
}

/**
 * The value read as an unsigned 32-bit integer.
 * @param {number} value
 */
export function unsigned(value) {
  return value >>> 0;
}

/** @typedef {"hex" | "signed" | "unsigned"} Radix */

/**
 * @param {number} value
 * @param {Radix} radix
 */
export function formatValue(value, radix) {
  if (radix === "hex") return hex(value);
  if (radix === "unsigned") return String(unsigned(value));
  return String(signed(value));
}

/**
 * "x5"
 * @param {number} index
 */
export function registerName(index) {
  return `x${index}`;
}

/**
 * The calling-convention name: "t0" for x5.
 * @param {number} index
 */
export function abiName(index) {
  return ABI_NAMES[index] ?? registerName(index);
}

/**
 * "1 byte", "4 bytes"
 * @param {number} count
 * @param {string} noun
 */
export function plural(count, noun) {
  return `${count} ${noun}${count === 1 ? "" : "s"}`;
}
