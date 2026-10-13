// Loads the real simulator timelines in tests/fixtures (npm run fixtures regenerates them).

import { readdirSync, readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import { parseTimeline } from "../../src/timeline/parse.js";

const fixturesDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "fixtures");

/**
 * The raw fixture: { source, timeline } where timeline is the document rvsim printed.
 * @param {string} name
 * @returns {{ source: string, timeline: any }}
 */
export function loadFixture(name) {
  return JSON.parse(readFileSync(path.join(fixturesDir, `${name}.json`), "utf8"));
}

/**
 * A parsed timeline that ran (not an assembly failure).
 * @param {string} name
 * @returns {import("../../src/timeline/types.js").Timeline}
 */
export function loadTimeline(name) {
  const result = parseTimeline(loadFixture(name).timeline);
  if (!result.ok) throw new Error(`${name} did not assemble`);
  return result.value;
}

/** Names of every fixture whose program assembled. */
export function timelineFixtureNames() {
  return readdirSync(fixturesDir)
    .filter((file) => file.endsWith(".json"))
    .map((file) => path.basename(file, ".json"))
    .filter((name) => loadFixture(name).timeline.ok === true)
    .sort();
}
