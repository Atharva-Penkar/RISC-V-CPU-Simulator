import assert from "node:assert/strict";
import { chmod, mkdtemp, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import { after, before, describe, test } from "node:test";

import { createApp } from "../src/app.js";
import { loadConfig } from "../src/config.js";
import { loadExamples } from "../src/examples.js";
import { Simulator } from "../src/simulator.js";

/** Starts the API on a free port; returns { url, close }. */
async function startServer(overrides = {}) {
  const config = { ...loadConfig(), ...overrides };
  const app = createApp({ config, simulator: new Simulator(config), examples: await loadExamples(config.examplesDir) });
  const server = await new Promise((resolve) => {
    const s = app.listen(0, () => resolve(s));
  });
  return { url: `http://127.0.0.1:${server.address().port}`, close: () => new Promise((r) => server.close(r)) };
}

async function post(url, body) {
  const response = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: typeof body === "string" ? body : JSON.stringify(body),
  });
  return { status: response.status, body: await response.json() };
}

/** Rebuilds the register file from the per-cycle writes, as a frontend would. */
function replayRegisters(timeline) {
  const registers = new Array(32).fill(0);
  for (const cycle of timeline.cycles) {
    if (cycle.registerWrite) registers[cycle.registerWrite.register] = cycle.registerWrite.value;
  }
  return registers;
}

describe("API", () => {
  let server;
  before(async () => {
    server = await startServer();
    const health = await (await fetch(`${server.url}/api/health`)).json();
    assert.equal(health.status, "ok", "build rvsim and rvasm first: npm run build:simulator");
  });
  after(() => server.close());

  test("health reports the simulator and the limits", async () => {
    const response = await fetch(`${server.url}/api/health`);
    const body = await response.json();
    assert.equal(response.status, 200);
    assert.deepEqual(body.simulator, { rvsim: true, rvasm: true });
    assert.equal(body.limits.maxCyclesLimit, 10000);
    assert.equal(response.headers.get("access-control-allow-origin"), "*");
  });

  test("large responses are compressed", async () => {
    const example = await (await fetch(`${server.url}/api/examples/fibonacci`)).json();
    const response = await fetch(`${server.url}/api/simulate`, {
      method: "POST",
      headers: { "Content-Type": "application/json", "Accept-Encoding": "gzip" },
      body: JSON.stringify({ source: example.source }),
    });
    assert.equal(response.headers.get("content-encoding"), "gzip");
    assert.equal((await response.json()).format, "rv-timeline");
  });

  test("examples are listed with descriptions and can be fetched", async () => {
    const list = await (await fetch(`${server.url}/api/examples`)).json();
    const fibonacci = list.examples.find((e) => e.id === "fibonacci");
    assert.ok(fibonacci && fibonacci.description.includes("Fibonacci"));
    const one = await (await fetch(`${server.url}/api/examples/fibonacci`)).json();
    assert.match(one.source, /blt/);
    assert.equal((await fetch(`${server.url}/api/examples/nope`)).status, 404);
  });

  test("assemble returns instructions with their source locations", async () => {
    const { status, body } = await post(`${server.url}/api/assemble`, { source: "addi x1, x0, 5\n  add x2, x1, x1" });
    assert.equal(status, 200);
    assert.equal(body.format, "rv-program");
    assert.equal(body.instructions.length, 2);
    assert.deepEqual(body.instructions[1].source, { line: 2, column: 3 });
    assert.equal(body.instructions[1].text, "ADD x2, x1, x1");
    assert.equal(body.instructions[0].word, "0x00500093");
  });

  test("assembly errors are 422 with every diagnostic", async () => {
    for (const route of ["assemble", "simulate"]) {
      const { status, body } = await post(`${server.url}/api/${route}`, { source: "foo x1\naddi x1, x0, 99999" });
      assert.equal(status, 422);
      assert.equal(body.error.code, "ASSEMBLY_FAILED");
      assert.deepEqual(
        body.error.details.diagnostics.map((d) => [d.line, d.column]),
        [
          [1, 1],
          [2, 14],
        ],
      );
    }
  });

  test("simulate returns a consistent timeline", async () => {
    const example = await (await fetch(`${server.url}/api/examples/forwarding`)).json();
    const { status, body } = await post(`${server.url}/api/simulate`, { source: example.source });
    assert.equal(status, 200);
    assert.equal(body.format, "rv-timeline");
    assert.equal(body.outcome.status, "finished");
    assert.equal(body.cycles.length, body.stats.cycles);
    assert.equal(body.stats.stalls, 2);

    const instanceIds = new Set(body.instances.map((i) => i.id));
    for (const [index, cycle] of body.cycles.entries()) {
      assert.equal(cycle.cycle, index + 1);
      assert.deepEqual(Object.keys(cycle.stages), ["IF", "ID", "EX", "MEM", "WB"]);
      for (const slot of Object.values(cycle.stages)) {
        if (slot?.instance !== undefined) assert.ok(instanceIds.has(slot.instance));
      }
      for (const f of cycle.forwarding) assert.ok(instanceIds.has(f.producer) && instanceIds.has(f.consumer));
    }
    for (const instance of body.instances) {
      if (instance.instruction !== null) assert.ok(body.program.instructions[instance.instruction]);
    }
    assert.ok(body.cycles.some((c) => c.forwarding.some((f) => f.from === "EX/MEM")));
    assert.ok(body.cycles.some((c) => c.forwarding.some((f) => f.from === "MEM/WB")));
    assert.ok(body.cycles.some((c) => c.stall !== null));
    assert.ok(body.cycles.some((c) => c.flush !== null));
    assert.deepEqual(replayRegisters(body), body.final.registers);
  });

  test("halts, faults and the cycle limit are successful runs with an outcome", async () => {
    const halted = await post(`${server.url}/api/simulate`, { source: "addi x1, x0, 1\nebreak\naddi x2, x0, 2" });
    assert.equal(halted.status, 200);
    assert.equal(halted.body.outcome.status, "halted");
    assert.equal(halted.body.outcome.haltPc, 4);

    const faulted = await post(`${server.url}/api/simulate`, { source: "addi x1, x0, 256\nlw x2, 0(x1)" });
    assert.equal(faulted.body.outcome.status, "faulted");
    assert.deepEqual(faulted.body.outcome.fault, { kind: "load-out-of-range", pc: 4, address: 256 });
    assert.equal(faulted.body.final.memory.bytes, 256);

    const looped = await post(`${server.url}/api/simulate`, { source: "l: jal x0, l", maxCycles: 25 });
    assert.equal(looped.body.outcome.status, "cycle-limit");
    assert.equal(looped.body.cycles.length, 25);
  });

  test("requests are validated", async () => {
    assert.equal((await post(`${server.url}/api/simulate`, {})).status, 400);
    assert.equal((await post(`${server.url}/api/simulate`, { source: 5 })).status, 400);
    assert.equal((await post(`${server.url}/api/simulate`, { source: "ebreak", maxCycles: 0 })).status, 400);
    assert.equal((await post(`${server.url}/api/simulate`, { source: "ebreak", maxCycles: 10001 })).status, 400);
    assert.equal((await post(`${server.url}/api/simulate`, { source: "ebreak", maxCycles: 1.5 })).status, 400);
    const invalid = await post(`${server.url}/api/simulate`, "{not json");
    assert.equal(invalid.status, 400);
    assert.equal(invalid.body.error.code, "INVALID_JSON");
    const huge = await post(`${server.url}/api/simulate`, { source: "#".repeat(70 * 1024) });
    assert.equal(huge.status, 413);
    assert.equal((await fetch(`${server.url}/api/nothing`)).status, 404);
    const preflight = await fetch(`${server.url}/api/simulate`, { method: "OPTIONS" });
    assert.equal(preflight.status, 204);
  });
});

describe("simulator failures", () => {
  let scripts;
  before(async () => {
    scripts = await mkdtemp(path.join(tmpdir(), "rvsim-fake-"));
    const write = async (name, body) => {
      const file = path.join(scripts, name);
      await writeFile(file, `#!/bin/sh\n${body}\n`);
      await chmod(file, 0o755);
      return file;
    };
    scripts = {
      slow: await write("slow.sh", "sleep 5"),
      garbage: await write("garbage.sh", "echo not json"),
      crash: await write("crash.sh", `echo '{"format":"rv-timeline","ok":true}'; exit 9`),
    };
  });

  test("a run that takes too long is stopped", async () => {
    const server = await startServer({ rvsimPath: scripts.slow, runTimeoutMs: 200 });
    const { status, body } = await post(`${server.url}/api/simulate`, { source: "ebreak" });
    await server.close();
    assert.equal(status, 504);
    assert.equal(body.error.code, "SIMULATION_TIMEOUT");
  });

  test("a missing executable is reported as unavailable", async () => {
    const server = await startServer({ rvsimPath: "/nonexistent/rvsim" });
    const { status, body } = await post(`${server.url}/api/simulate`, { source: "ebreak" });
    const health = await (await fetch(`${server.url}/api/health`)).json();
    await server.close();
    assert.equal(status, 503);
    assert.equal(body.error.code, "SIMULATOR_UNAVAILABLE");
    assert.equal(health.status, "degraded");
  });

  test("invalid output or an unexpected exit code is an internal error", async () => {
    for (const fake of [scripts.garbage, scripts.crash]) {
      const server = await startServer({ rvsimPath: fake });
      const { status, body } = await post(`${server.url}/api/simulate`, { source: "ebreak" });
      await server.close();
      assert.equal(status, 500);
      assert.equal(body.error.code, "SIMULATOR_ERROR");
    }
  });

  test("output beyond the size limit is rejected", async () => {
    const server = await startServer({ maxOutputBytes: 1000 });
    const { status, body } = await post(`${server.url}/api/simulate`, { source: "l: jal x0, l", maxCycles: 500 });
    await server.close();
    assert.equal(status, 413);
    assert.equal(body.error.code, "OUTPUT_TOO_LARGE");
  });
});
