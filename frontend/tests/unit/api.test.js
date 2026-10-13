import assert from "node:assert/strict";
import { afterEach, describe, test } from "node:test";

import { ApiError, isAbortError, request } from "../../src/api/client.js";
import { assemblyDiagnostics, simulate } from "../../src/api/endpoints.js";
import { loadFixture } from "./fixtures.js";

const realFetch = globalThis.fetch;

/**
 * Replaces fetch with one that answers every request with this status and body.
 * @param {number} status
 * @param {unknown} body
 * @param {(url: string, init: RequestInit) => void} [inspect]
 */
function respondWith(status, body, inspect) {
  globalThis.fetch = /** @type {typeof fetch} */ (
    async (input, init) => {
      inspect?.(String(input), /** @type {RequestInit} */ (init));
      return new Response(typeof body === "string" ? body : JSON.stringify(body), { status });
    }
  );
}

afterEach(() => {
  globalThis.fetch = realFetch;
});

describe("request", () => {
  test("sends JSON and returns the parsed body", async () => {
    /** @type {RequestInit | undefined} */
    let sent;
    respondWith(200, { status: "ok" }, (url, init) => {
      assert.equal(url, "/api/simulate");
      sent = init;
    });
    assert.deepEqual(await request("/api/simulate", { method: "POST", body: { source: "ebreak" } }), { status: "ok" });
    assert.equal(sent?.method, "POST");
    assert.equal(sent?.body, '{"source":"ebreak"}');
    assert.deepEqual(sent?.headers, { "Content-Type": "application/json" });
  });

  test("maps the backend's error body to ApiError", async () => {
    respondWith(504, { error: { code: "SIMULATION_TIMEOUT", message: "Too slow." } });
    await assert.rejects(request("/api/simulate"), (error) => {
      assert.ok(error instanceof ApiError);
      assert.equal(error.status, 504);
      assert.equal(error.code, "SIMULATION_TIMEOUT");
      assert.equal(error.message, "Too slow.");
      return true;
    });
  });

  test("handles error pages that are not JSON, and unreachable servers", async () => {
    respondWith(502, "<html>Bad gateway</html>");
    await assert.rejects(request("/api/health"), { code: "HTTP_ERROR", status: 502 });
    respondWith(200, "not json");
    await assert.rejects(request("/api/health"), { code: "INVALID_RESPONSE" });
    globalThis.fetch = async () => {
      throw new TypeError("Failed to fetch");
    };
    await assert.rejects(request("/api/health"), { code: "NETWORK_ERROR", status: 0 });
  });

  test("lets cancellation through as an AbortError", async () => {
    globalThis.fetch = async (_input, init) => {
      init?.signal?.throwIfAborted();
      return new Response("{}");
    };
    const controller = new AbortController();
    controller.abort();
    await assert.rejects(request("/api/health", { signal: controller.signal }), (error) => isAbortError(error));
  });
});

describe("endpoints", () => {
  test("simulate returns a validated timeline", async () => {
    const { timeline } = loadFixture("forwarding");
    respondWith(200, timeline);
    const result = await simulate("…", { maxCycles: 100 });
    assert.equal(result.stats.cycles, timeline.stats.cycles);
  });

  test("assembly errors carry their diagnostics", async () => {
    const diagnostics = [{ line: 2, column: 1, message: "unknown instruction 'foo'" }];
    respondWith(422, { error: { code: "ASSEMBLY_FAILED", message: "The program has assembly errors.", details: { stage: "assemble", diagnostics } } });
    const error = await simulate("foo").catch((e) => e);
    assert.deepEqual(assemblyDiagnostics(error), diagnostics);
    assert.equal(assemblyDiagnostics(new Error("other")), null);
  });

  test("a response in the wrong format is an INVALID_RESPONSE error", async () => {
    respondWith(200, { format: "something-else", version: 1, ok: true });
    await assert.rejects(simulate("ebreak"), { code: "INVALID_RESPONSE" });
  });
});
