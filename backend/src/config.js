import path from "node:path";
import { fileURLToPath } from "node:url";

const backendDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const repoDir = path.resolve(backendDir, "..");

function integer(name, fallback) {
  const raw = process.env[name];
  if (raw === undefined || raw === "") return fallback;
  const value = Number.parseInt(raw, 10);
  if (!Number.isInteger(value) || value <= 0) {
    throw new Error(`${name} must be a positive integer, got "${raw}"`);
  }
  return value;
}

/** Settings read from the environment, with defaults for running from a repository checkout. */
export function loadConfig() {
  return {
    port: integer("PORT", 3001),
    // Executables built from the C++ sources (npm run build:simulator).
    rvsimPath: process.env.RVSIM_PATH || path.join(repoDir, "build", "rvsim"),
    rvasmPath: process.env.RVASM_PATH || path.join(repoDir, "build", "rvasm"),
    examplesDir: process.env.EXAMPLES_DIR || path.join(repoDir, "tests", "programs"),
    // Value of Access-Control-Allow-Origin; "*" allows any frontend origin.
    corsOrigin: process.env.CORS_ORIGIN || "*",
    maxSourceBytes: integer("MAX_SOURCE_BYTES", 64 * 1024),
    defaultMaxCycles: integer("DEFAULT_MAX_CYCLES", 1000),
    maxCyclesLimit: integer("MAX_CYCLES_LIMIT", 10000),
    runTimeoutMs: integer("RUN_TIMEOUT_MS", 10000),
    maxOutputBytes: integer("MAX_OUTPUT_BYTES", 32 * 1024 * 1024),
    maxConcurrentRuns: integer("MAX_CONCURRENT_RUNS", 4),
  };
}
