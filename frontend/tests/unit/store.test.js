import assert from "node:assert/strict";
import { describe, test } from "node:test";

import { createAppStore, IDEAL_LATENCIES, lastCycle } from "../../src/state/store.js";
import { loadTimeline } from "./fixtures.js";

/** A store holding a finished run of the forwarding fixture. */
function storeWithRun() {
  const store = createAppStore();
  store.actions.runSucceeded(loadTimeline("forwarding"));
  return store;
}

describe("app store", () => {
  test("notifies subscribers and replaces the state object", () => {
    const store = createAppStore();
    let calls = 0;
    const unsubscribe = store.subscribe(() => calls++);
    const before = store.getState();
    store.actions.setSource("ebreak\n");
    assert.equal(calls, 1);
    assert.notEqual(store.getState(), before);
    store.actions.setSource("ebreak\n");
    assert.equal(calls, 1, "an unchanged source is not a change");
    unsubscribe();
    store.actions.setSource("nop\n");
    assert.equal(calls, 1);
  });

  test("seeking and stepping stay within the run", () => {
    const store = storeWithRun();
    const last = lastCycle(store.getState());
    assert.ok(last > 10);
    store.actions.seek(5);
    assert.equal(store.getState().cycle, 5);
    store.actions.step(-10);
    assert.equal(store.getState().cycle, 0);
    store.actions.step(Infinity);
    assert.equal(store.getState().cycle, last);
    store.actions.seek(last + 100);
    assert.equal(store.getState().cycle, last);
  });

  test("playback runs to the last cycle, then restarts from the beginning", () => {
    const store = storeWithRun();
    const last = lastCycle(store.getState());
    store.actions.seek(last - 2);
    store.actions.play();
    store.actions.tick();
    assert.equal(store.getState().playing, true);
    store.actions.tick();
    assert.deepEqual([store.getState().cycle, store.getState().playing], [last, false]);
    store.actions.togglePlay();
    assert.deepEqual([store.getState().cycle, store.getState().playing], [0, true]);
    store.actions.step(1);
    assert.equal(store.getState().playing, false, "stepping pauses");
  });

  test("cannot play without a run", () => {
    const store = createAppStore();
    store.actions.play();
    assert.equal(store.getState().playing, false);
  });

  test("a failed run keeps the previous timeline, marked stale", () => {
    const store = storeWithRun();
    const timeline = store.getState().timeline;
    store.actions.seek(7);
    store.actions.setSource("foo x1\n");
    assert.equal(store.getState().stale, true);
    const diagnostics = [{ line: 1, column: 1, message: "unknown instruction 'foo'" }];
    store.actions.runFailed({ diagnostics });
    assert.equal(store.getState().status, "assembly-error");
    assert.deepEqual(store.getState().diagnostics, diagnostics);
    assert.equal(store.getState().timeline, timeline);
    assert.equal(store.getState().cycle, 7);

    store.actions.runFailed({ message: "Could not reach the simulator." });
    assert.equal(store.getState().status, "error");
    assert.deepEqual(store.getState().diagnostics, []);
    assert.equal(store.getState().error, "Could not reach the simulator.");
  });

  test("a new run keeps the current cycle where it can", () => {
    const store = storeWithRun();
    store.actions.seek(20);
    store.actions.runSucceeded(loadTimeline("halt"));
    const state = store.getState();
    assert.equal(state.status, "done");
    assert.equal(state.stale, false);
    assert.equal(state.cycle, Math.min(20, lastCycle(state)));
  });
});

describe("latencies", () => {
  test("start ideal, change one at a time and mark the run stale", () => {
    const store = storeWithRun();
    assert.deepEqual(store.getState().latencies, IDEAL_LATENCIES);
    let calls = 0;
    store.subscribe(() => calls++);
    store.actions.setLatencies({ multiply: 3 });
    assert.deepEqual(store.getState().latencies, { multiply: 3, divide: 1, load: 1, store: 1 });
    assert.equal(store.getState().stale, true);
    store.actions.setLatencies({ multiply: 3 });
    assert.equal(calls, 1, "an unchanged value is not a change");
  });
});

describe("forwarding", () => {
  test("is on by default; turning it off marks the run stale", () => {
    const store = storeWithRun();
    assert.equal(store.getState().forwarding, true);
    store.actions.setForwarding(false);
    assert.equal(store.getState().forwarding, false);
    assert.equal(store.getState().stale, true);
  });
});

describe("loaded example", () => {
  test("is kept while the program is unchanged and dropped on the first edit", () => {
    const store = createAppStore();
    assert.equal(store.getState().example?.id, "demo");
    store.actions.loadProgram("ebreak\n", { id: "halt", description: "Stops." });
    assert.deepEqual(store.getState().example, { id: "halt", description: "Stops." });
    store.actions.setSource("ebreak\n");
    assert.equal(store.getState().example?.id, "halt", "the editor echoing the loaded text is not an edit");
    store.actions.setSource("ebreak\nnop\n");
    assert.equal(store.getState().example, null);
  });
});
