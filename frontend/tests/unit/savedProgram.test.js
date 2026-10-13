import assert from "node:assert/strict";
import { afterEach, beforeEach, describe, mock, test } from "node:test";

import { loadSavedProgram, persistProgram, SAVE_DELAY_MS, saveProgram, STORAGE_KEY } from "../../src/state/savedProgram.js";
import { createAppStore, DEFAULT_SOURCE } from "../../src/state/store.js";

/** An in-memory stand-in for localStorage. */
function memoryStorage(initial = {}) {
  const items = new Map(Object.entries(initial));
  return {
    getItem: (/** @type {string} */ key) => items.get(key) ?? null,
    setItem: (/** @type {string} */ key, /** @type {string} */ value) => void items.set(key, value),
    items,
  };
}

/** A storage that throws, like a blocked or full localStorage. */
const brokenStorage = {
  getItem() {
    throw new Error("SecurityError");
  },
  setItem() {
    throw new Error("QuotaExceededError");
  },
};

describe("saved program", () => {
  beforeEach(() => mock.timers.enable({ apis: ["setTimeout"] }));
  afterEach(() => mock.timers.reset());

  test("restores the saved text into the editor, without the demo's caption", () => {
    const storage = memoryStorage({ [STORAGE_KEY]: "addi x1, x0, 7\n" });
    const store = createAppStore();
    persistProgram(store, storage);
    assert.equal(store.getState().source, "addi x1, x0, 7\n");
    assert.equal(store.getState().example, null);
  });

  test("keeps the demo when nothing, or only the demo, was saved", () => {
    for (const storage of [memoryStorage(), memoryStorage({ [STORAGE_KEY]: DEFAULT_SOURCE })]) {
      const store = createAppStore();
      persistProgram(store, storage);
      assert.equal(store.getState().source, DEFAULT_SOURCE);
      assert.equal(store.getState().example?.id, "demo");
    }
  });

  test("saves once typing pauses, and only when the text changed", () => {
    const storage = memoryStorage();
    const setItem = mock.method(storage, "setItem");
    const store = createAppStore();
    persistProgram(store, storage);
    store.actions.setSource("add");
    store.actions.setSource("addi x1");
    mock.timers.tick(SAVE_DELAY_MS - 1);
    assert.equal(setItem.mock.callCount(), 0, "not while typing");
    mock.timers.tick(1);
    assert.equal(storage.items.get(STORAGE_KEY), "addi x1");
    assert.equal(setItem.mock.callCount(), 1);
    store.actions.seek(3);
    mock.timers.tick(SAVE_DELAY_MS);
    assert.equal(setItem.mock.callCount(), 1, "other state changes are not saved");
  });

  test("saves a loaded example too, and flushes pending text when stopped", () => {
    const storage = memoryStorage();
    const store = createAppStore();
    const stop = persistProgram(store, storage);
    store.actions.loadProgram("ebreak\n", { id: "halt", description: "" });
    stop();
    assert.equal(storage.items.get(STORAGE_KEY), "ebreak\n");
  });

  test("works without storage, or with storage that throws", () => {
    assert.equal(loadSavedProgram(null), null);
    assert.equal(loadSavedProgram(brokenStorage), null);
    assert.doesNotThrow(() => saveProgram("x", brokenStorage));
    const store = createAppStore();
    persistProgram(store, brokenStorage);
    store.actions.setSource("addi x1, x0, 1");
    assert.doesNotThrow(() => mock.timers.tick(SAVE_DELAY_MS));
    assert.equal(store.getState().source, "addi x1, x0, 1");
  });
});
