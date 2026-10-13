/** @typedef {import("./types.js").Timeline} Timeline */
/** @typedef {import("./types.js").Program} Program */
/** @typedef {import("./types.js").Diagnostic} Diagnostic */

export const FORMAT_VERSION = 2;

/** The document is not one this viewer understands (wrong format, version or shape). */
export class TimelineFormatError extends Error {
  /** @param {string} message */
  constructor(message) {
    super(message);
    this.name = "TimelineFormatError";
  }
}

/**
 * @template T
 * @typedef {{ ok: true, value: T } | { ok: false, diagnostics: Diagnostic[] }} ParseResult
 */

/**
 * Checks the header shared by every document and splits off assembly errors.
 * @param {any} document
 * @param {string} format
 * @returns {{ ok: true } | { ok: false, diagnostics: Diagnostic[] }}
 */
function checkHeader(document, format) {
  if (document === null || typeof document !== "object") throw new TimelineFormatError("The document is not an object.");
  if (document.format !== format) {
    throw new TimelineFormatError(`Expected a "${format}" document, got "${document.format}".`);
  }
  if (document.version !== FORMAT_VERSION) {
    throw new TimelineFormatError(`Unsupported ${format} version ${document.version}; this viewer reads version ${FORMAT_VERSION}.`);
  }
  if (document.ok === false) {
    const diagnostics = document.error?.diagnostics;
    if (!Array.isArray(diagnostics)) throw new TimelineFormatError("A failed document has no diagnostics.");
    return { ok: false, diagnostics };
  }
  if (document.ok !== true) throw new TimelineFormatError('The document has no "ok" field.');
  return { ok: true };
}

/**
 * @param {unknown} value
 * @param {string} what
 */
function requireArray(value, what) {
  if (!Array.isArray(value)) throw new TimelineFormatError(`The document has no ${what} array.`);
}

/**
 * Validates an "rv-timeline" document. The selectors index instances and cycles by position,
 * so their ids must match it.
 * @param {any} document
 * @returns {ParseResult<Timeline>}
 */
export function parseTimeline(document) {
  const header = checkHeader(document, "rv-timeline");
  if (!header.ok) return header;

  requireArray(document.program?.instructions, "program.instructions");
  requireArray(document.instances, "instances");
  requireArray(document.cycles, "cycles");
  requireArray(document.final?.registers, "final.registers");
  requireArray(document.final?.memory?.words, "final.memory.words");
  if (!document.outcome || !document.stats) throw new TimelineFormatError("The document has no outcome or stats.");

  document.instances.forEach((/** @type {any} */ instance, /** @type {number} */ index) => {
    if (instance.id !== index) throw new TimelineFormatError(`Instance ${index} has id ${instance.id}.`);
  });
  document.cycles.forEach((/** @type {any} */ cycle, /** @type {number} */ index) => {
    if (cycle.cycle !== index + 1) throw new TimelineFormatError(`Cycle ${index + 1} is numbered ${cycle.cycle}.`);
  });
  return { ok: true, value: /** @type {Timeline} */ (document) };
}

/**
 * Validates an "rv-program" document.
 * @param {any} document
 * @returns {ParseResult<Program>}
 */
export function parseProgram(document) {
  const header = checkHeader(document, "rv-program");
  if (!header.ok) return header;
  requireArray(document.instructions, "instructions");
  return { ok: true, value: /** @type {Program} */ (document) };
}
