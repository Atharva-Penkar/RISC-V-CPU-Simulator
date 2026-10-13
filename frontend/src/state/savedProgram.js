// Keeps the editor text in the browser's localStorage, so a refresh or a closed tab does not lose it.
// Storage can be missing or throw (private windows, blocked site data, a full quota); the editor then
// works as before, just without saving.

import { DEFAULT_SOURCE } from "./store.js";

/** localStorage key; versioned so a future format change can ignore old entries. */
export const STORAGE_KEY = "riscv-sim:program:v1";

/** How long typing must pause before the text is saved, in milliseconds. */
export const SAVE_DELAY_MS = 300;

/** @typedef {Pick<Storage, "getItem" | "setItem">} ProgramStorage */

/** @returns {ProgramStorage | null} */
function browserStorage() {
  try {
    return globalThis.localStorage ?? null;
  } catch {
    return null;
  }
}

/**
 * The saved program, or null when nothing (or only the built-in demo) was saved.
 * @param {ProgramStorage | null} [storage]
 * @returns {string | null}
 */
export function loadSavedProgram(storage = browserStorage()) {
  try {
    const source = storage?.getItem(STORAGE_KEY);
    return typeof source === "string" && source !== DEFAULT_SOURCE ? source : null;
  } catch {
    return null;
  }
}

/**
 * @param {string} source
 * @param {ProgramStorage | null} [storage]
 */
export function saveProgram(source, storage = browserStorage()) {
  try {
    storage?.setItem(STORAGE_KEY, source);
  } catch {
    // Saving is a convenience; the editor keeps working without it.
  }
}

/**
 * Puts the saved program in the editor (before the first run), then saves the editor text whenever it
 * changes and typing pauses. Returns a function that stops saving.
 * @param {import("./store.js").AppStore} store
 * @param {ProgramStorage | null} [storage]
 */
export function persistProgram(store, storage = browserStorage()) {
  const saved = loadSavedProgram(storage);
  if (saved !== null) store.actions.loadProgram(saved, null);

  let lastSaved = store.getState().source;
  /** @type {ReturnType<typeof setTimeout> | undefined} */
  let timer;
  const flush = () => {
    clearTimeout(timer);
    const { source } = store.getState();
    if (source !== lastSaved) {
      saveProgram(source, storage);
      lastSaved = source;
    }
  };
  const unsubscribe = store.subscribe(() => {
    if (store.getState().source === lastSaved) return;
    clearTimeout(timer);
    timer = setTimeout(flush, SAVE_DELAY_MS);
  });
  // Typing and closing the tab within the delay must not lose the last keystrokes.
  globalThis.addEventListener?.("pagehide", flush);

  return () => {
    flush();
    unsubscribe();
    globalThis.removeEventListener?.("pagehide", flush);
  };
}
