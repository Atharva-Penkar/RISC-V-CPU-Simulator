// Regenerates tests/fixtures/*.json: real timelines from the C++ simulator, for the unit tests.
// Run after changing the simulator or the JSON format:
//   cmake --build ../build && npm run fixtures
// RVSIM_PATH overrides the simulator executable (default ../build/rvsim).

import { spawnSync } from "node:child_process";
import { mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const frontendDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const repoDir = path.resolve(frontendDir, "..");
const rvsim = process.env.RVSIM_PATH || path.join(repoDir, "build", "rvsim");
const programsDir = path.join(repoDir, "tests", "programs");
const fixturesDir = path.join(frontendDir, "tests", "fixtures");

/** Programs that cover outcomes the example programs do not. */
const extraPrograms = {
  "outcome-halt": "addi x1, x0, 3\nsw x1, 0(x0)\nebreak\naddi x2, x0, 1\n",
  "outcome-load-fault": "addi x1, x0, 256\nlw x2, 0(x1)\naddi x3, x0, 1\n",
  "outcome-misaligned-store": "addi x1, x0, 7\nsh x1, 3(x0)\n",
  "outcome-cycle-limit": "loop: addi x1, x1, 1\njal x0, loop\n",
  "assembly-errors": "addi x1, x0, 5\nfoo x1, x2\nadd x2, x1\n",
};

/**
 * @param {string} name
 * @param {string} source
 */
function generate(name, source) {
  const result = spawnSync(rvsim, ["--json", "--max-cycles", "120"], { input: source, encoding: "utf8" });
  if (result.error) throw new Error(`Cannot run ${rvsim}: ${result.error.message}. Build it with cmake first.`);
  // Exit status 0 finished/halted, 1 assembly errors, 2 cycle limit, 3 fault; all print a JSON document.
  const document = JSON.parse(result.stdout);
  writeFileSync(path.join(fixturesDir, `${name}.json`), JSON.stringify({ source, timeline: document }) + "\n");
  const summary = document.ok ? `${document.outcome.status}, ${document.stats.cycles} cycles` : "assembly errors";
  console.log(`${name.padEnd(26)} ${summary}`);
}

mkdirSync(fixturesDir, { recursive: true });
for (const file of readdirSync(programsDir).filter((f) => f.endsWith(".s")).sort()) {
  generate(path.basename(file, ".s"), readFileSync(path.join(programsDir, file), "utf8"));
}
for (const [name, source] of Object.entries(extraPrograms)) generate(name, source);
