// The simulator API's endpoints. Responses are validated before they reach the rest of the app.

import { parseProgram, parseTimeline, TimelineFormatError } from "../timeline/parse.js";
import { ApiError, request } from "./client.js";

/** @typedef {import("../timeline/types.js").Timeline} Timeline */
/** @typedef {import("../timeline/types.js").Program} Program */
/** @typedef {import("../timeline/types.js").Diagnostic} Diagnostic */

/**
 * The assembly errors carried by an ASSEMBLY_FAILED error, or null for any other error.
 * @param {unknown} error
 * @returns {Diagnostic[] | null}
 */
export function assemblyDiagnostics(error) {
  if (error instanceof ApiError && error.code === "ASSEMBLY_FAILED" && Array.isArray(error.details?.diagnostics)) {
    return error.details.diagnostics;
  }
  return null;
}

/**
 * Turns a parse result into its value, or into the same ApiError the server sends for assembly errors.
 * @template T
 * @param {import("../timeline/parse.js").ParseResult<T>} result
 * @returns {T}
 */
function unwrap(result) {
  if (result.ok) return result.value;
  throw new ApiError(422, "ASSEMBLY_FAILED", "The program has assembly errors.", { diagnostics: result.diagnostics });
}

/**
 * @param {() => T} parse
 * @returns {T}
 * @template T
 */
function validated(parse) {
  try {
    return parse();
  } catch (error) {
    if (error instanceof TimelineFormatError) throw new ApiError(200, "INVALID_RESPONSE", error.message);
    throw error;
  }
}

/**
 * Runs a program on the pipeline and returns every recorded cycle.
 * Throws ApiError ASSEMBLY_FAILED (see assemblyDiagnostics) when the program does not assemble.
 * @param {string} source
 * @param {{ maxCycles?: number, signal?: AbortSignal }} [options]
 * @returns {Promise<Timeline>}
 */
export async function simulate(source, { maxCycles, signal } = {}) {
  const document = await request("/api/simulate", { method: "POST", body: { source, maxCycles }, signal });
  return validated(() => unwrap(parseTimeline(document)));
}

/**
 * Assembles a program without running it: instructions, encodings and source locations.
 * @param {string} source
 * @param {{ signal?: AbortSignal }} [options]
 * @returns {Promise<Program>}
 */
export async function assemble(source, { signal } = {}) {
  const document = await request("/api/assemble", { method: "POST", body: { source }, signal });
  return validated(() => unwrap(parseProgram(document)));
}

/**
 * @typedef {object} ExampleSummary
 * @property {string} id
 * @property {string} description
 */

/**
 * @param {{ signal?: AbortSignal }} [options]
 * @returns {Promise<ExampleSummary[]>}
 */
export async function listExamples({ signal } = {}) {
  const data = await request("/api/examples", { signal });
  return Array.isArray(data.examples) ? data.examples : [];
}

/**
 * @param {string} id
 * @param {{ signal?: AbortSignal }} [options]
 * @returns {Promise<ExampleSummary & { source: string }>}
 */
export async function getExample(id, { signal } = {}) {
  return request(`/api/examples/${encodeURIComponent(id)}`, { signal });
}

/**
 * @typedef {object} Health
 * @property {"ok" | "degraded"} status
 * @property {{ rvsim: boolean, rvasm: boolean }} simulator
 * @property {{ maxSourceBytes: number, defaultMaxCycles: number, maxCyclesLimit: number }} limits
 */

/**
 * @param {{ signal?: AbortSignal }} [options]
 * @returns {Promise<Health>}
 */
export async function health({ signal } = {}) {
  return request("/api/health", { signal });
}
