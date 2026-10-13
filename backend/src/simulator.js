import { spawn } from "node:child_process";
import { access, constants } from "node:fs/promises";

/** An error with the HTTP status the API should answer with. */
export class ApiError extends Error {
  constructor(status, code, message, details) {
    super(message);
    this.status = status;
    this.code = code;
    this.details = details;
  }
}

/** Limits how many simulator processes run at once; extra runs wait for a free slot. */
class Semaphore {
  constructor(limit) {
    this.limit = limit;
    this.active = 0;
    this.waiting = [];
  }

  async acquire() {
    if (this.active < this.limit) {
      this.active++;
      return;
    }
    await new Promise((resolve) => this.waiting.push(resolve));
  }

  release() {
    const next = this.waiting.shift();
    if (next) next();
    else this.active--;
  }
}

/**
 * Runs an executable with the source on standard input and collects standard output.
 * The process is killed when it runs too long or writes too much.
 */
function runProcess(executable, args, input, { timeoutMs, maxOutputBytes }) {
  return new Promise((resolve, reject) => {
    const child = spawn(executable, args, { stdio: ["pipe", "pipe", "pipe"] });
    const chunks = [];
    let outputBytes = 0;
    let stderr = "";
    let settled = false;

    const settle = (callback, value) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      callback(value);
    };
    /*answers at once instead of waiting for the pipes to close, which a stuck grandchild could delay*/
    const abort = (error) => {
      child.kill("SIGKILL");
      child.stdout.destroy();
      child.stderr.destroy();
      settle(reject, error);
    };

    const timer = setTimeout(
      () => abort(new ApiError(504, "SIMULATION_TIMEOUT", `The simulation did not finish within ${timeoutMs} ms.`)),
      timeoutMs,
    );

    child.stdout.on("data", (chunk) => {
      if (settled) return;
      outputBytes += chunk.length;
      if (outputBytes > maxOutputBytes) {
        abort(
          new ApiError(413, "OUTPUT_TOO_LARGE", "The simulation produced too much data. Lower maxCycles or shorten the program."),
        );
        return;
      }
      chunks.push(chunk);
    });
    child.stderr.on("data", (chunk) => {
      if (stderr.length < 4096) stderr += chunk.toString();
    });
    child.on("error", (error) => {
      settle(
        reject,
        error.code === "ENOENT"
          ? new ApiError(503, "SIMULATOR_UNAVAILABLE", `Simulator executable not found: ${executable}`)
          : error,
      );
    });
    child.on("close", (exitCode) => {
      settle(resolve, { exitCode, stdout: Buffer.concat(chunks).toString("utf8"), stderr });
    });

    // A process that exits early (e.g. on a usage error) closes stdin; ignore that write error.
    child.stdin.on("error", () => {});
    child.stdin.end(input);
  });
}

/** Parses a tool's JSON document; anything else is an internal error. */
function parseDocument(result, tool) {
  try {
    return JSON.parse(result.stdout);
  } catch {
    throw new ApiError(500, "SIMULATOR_ERROR", `${tool} did not produce valid JSON (exit code ${result.exitCode}).`, {
      stderr: result.stderr.trim(),
    });
  }
}

/** Assembly errors become 422 responses; the document is passed through unchanged. */
function checkDocument(document, format, result, tool) {
  if (document?.format !== format) {
    throw new ApiError(500, "SIMULATOR_ERROR", `${tool} returned an unexpected document.`);
  }
  if (document.ok === false) {
    throw new ApiError(422, "ASSEMBLY_FAILED", "The program has assembly errors.", document.error);
  }
  if (result.exitCode !== 0 && result.exitCode !== 2 && result.exitCode !== 3) {
    throw new ApiError(500, "SIMULATOR_ERROR", `${tool} exited with code ${result.exitCode}.`, {
      stderr: result.stderr.trim(),
    });
  }
  return document;
}

/** Talks to the rvsim and rvasm executables. */
export class Simulator {
  constructor(config) {
    this.config = config;
    this.slots = new Semaphore(config.maxConcurrentRuns);
  }

  async available() {
    const check = (file) =>
      access(file, constants.X_OK).then(
        () => true,
        () => false,
      );
    const [rvsim, rvasm] = await Promise.all([check(this.config.rvsimPath), check(this.config.rvasmPath)]);
    return { rvsim, rvasm };
  }

  async run(executable, args, source) {
    await this.slots.acquire();
    try {
      return await runProcess(executable, args, source, {
        timeoutMs: this.config.runTimeoutMs,
        maxOutputBytes: this.config.maxOutputBytes,
      });
    } finally {
      this.slots.release();
    }
  }

  /** Assembles source: the "rv-program" document with every instruction and its source location. */
  async assemble(source) {
    const result = await this.run(this.config.rvasmPath, ["--json"], source);
    return checkDocument(parseDocument(result, "rvasm"), "rv-program", result, "rvasm");
  }

  /** Runs source on the pipeline: the "rv-timeline" document with every cycle recorded. */
  async simulate(source, maxCycles) {
    const result = await this.run(this.config.rvsimPath, ["--json", "--max-cycles", String(maxCycles)], source);
    return checkDocument(parseDocument(result, "rvsim"), "rv-timeline", result, "rvsim");
  }
}
