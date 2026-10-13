// App state: the program text, the latest simulation and the cycle being viewed.
// A small external store read through React's useSyncExternalStore; everything the page shows is derived
// from the timeline and the current cycle with the functions in timeline/.

import { useSyncExternalStore } from "react";

/** @typedef {import("../timeline/types.js").Timeline} Timeline */
/** @typedef {import("../timeline/types.js").Diagnostic} Diagnostic */

/** Playback speeds, in cycles per second. */
export const SPEEDS = [1, 2, 4, 8, 16];

export const DEFAULT_MAX_CYCLES = 1000;

export const DEFAULT_SOURCE = `# Sums 4 + 3 + 2 + 1 into memory, then reads it back.
        addi x1, x0, 4          # counter
        addi x2, x0, 0          # sum
loop:   add  x2, x2, x1         # x1 is forwarded from the instruction ahead
        addi x1, x1, -1
        bne  x1, x0, loop       # taken: the two instructions behind it are flushed
        sw   x2, 0[x0]
        lw   x3, 0[x0]
        addi x4, x3, 1          # load-use: waits one cycle for the load
        ebreak
`;

/**
 * @typedef {"idle" | "running" | "done" | "assembly-error" | "error"} RunStatus
 *
 * @typedef {object} AppState
 * @property {string} source         the program in the editor
 * @property {number} maxCycles      cycle limit sent with each run
 * @property {number} runId          bumped by retry() to run the same source again
 * @property {RunStatus} status      state of the latest run
 * @property {Diagnostic[]} diagnostics  assembly errors of the latest run
 * @property {string | null} error   message of the latest run's failure, other than assembly errors
 * @property {Timeline | null} timeline  the latest successful run; kept while an edit does not assemble
 * @property {boolean} stale         the timeline was produced from different source than the editor shows
 * @property {number} cycle          cycle being viewed: 0 is before the first cycle, timeline.cycles.length the last
 * @property {boolean} playing
 * @property {number} speed          cycles per second while playing
 */

/** @returns {AppState} */
function initialState() {
  return {
    source: DEFAULT_SOURCE,
    maxCycles: DEFAULT_MAX_CYCLES,
    runId: 0,
    status: "idle",
    diagnostics: [],
    error: null,
    timeline: null,
    stale: false,
    cycle: 0,
    playing: false,
    speed: 2,
  };
}

/**
 * @param {AppState} state
 * @returns {number} the last cycle that can be viewed
 */
export function lastCycle(state) {
  return state.timeline?.cycles.length ?? 0;
}

/**
 * @param {number} value
 * @param {number} min
 * @param {number} max
 */
function clamp(value, min, max) {
  return Math.min(Math.max(Math.trunc(value), min), max);
}

/**
 * Creates a store with its actions. The app uses one (appStore); tests make their own.
 * @param {Partial<AppState>} [overrides]
 */
export function createAppStore(overrides = {}) {
  /** @type {AppState} */
  let state = { ...initialState(), ...overrides };
  /** @type {Set<() => void>} */
  const listeners = new Set();

  /** @param {Partial<AppState> | ((state: AppState) => Partial<AppState>)} change */
  function set(change) {
    const next = typeof change === "function" ? change(state) : change;
    state = { ...state, ...next };
    for (const listener of listeners) listener();
  }

  const actions = {
    /** @param {string} source */
    setSource(source) {
      if (source !== state.source) set({ source, stale: state.timeline !== null });
    },

    /** @param {number} maxCycles */
    setMaxCycles(maxCycles) {
      set({ maxCycles });
    },

    /** Runs the current source again, e.g. after a network error. */
    retry() {
      set((s) => ({ runId: s.runId + 1 }));
    },

    runStarted() {
      set({ status: "running" });
    },

    /**
     * A run finished. The cycle being viewed is kept where possible, so editing a program keeps the
     * same moment on screen.
     * @param {Timeline} timeline
     */
    runSucceeded(timeline) {
      set((s) => {
        const cycle = clamp(s.cycle, 0, timeline.cycles.length);
        return {
          status: "done",
          diagnostics: [],
          error: null,
          timeline,
          stale: false,
          cycle,
          playing: s.playing && cycle < timeline.cycles.length,
        };
      });
    },

    /**
     * A run failed. The previous timeline stays on screen, marked stale.
     * @param {{ diagnostics?: Diagnostic[] | null, message?: string }} failure
     */
    runFailed({ diagnostics, message }) {
      set((s) =>
        diagnostics
          ? { status: "assembly-error", diagnostics, error: null, stale: s.timeline !== null }
          : { status: "error", diagnostics: [], error: message ?? "The simulation failed.", stale: s.timeline !== null },
      );
    },

    /** @param {number} cycle */
    seek(cycle) {
      set((s) => ({ cycle: clamp(cycle, 0, lastCycle(s)) }));
    },

    /** @param {number} delta */
    step(delta) {
      set((s) => ({ cycle: clamp(s.cycle + delta, 0, lastCycle(s)), playing: false }));
    },

    play() {
      set((s) => {
        const last = lastCycle(s);
        if (last === 0) return {};
        return { playing: true, cycle: s.cycle >= last ? 0 : s.cycle };
      });
    },

    pause() {
      set({ playing: false });
    },

    togglePlay() {
      if (state.playing) actions.pause();
      else actions.play();
    },

    /** One playback step; playback stops on the last cycle. */
    tick() {
      set((s) => {
        const last = lastCycle(s);
        const cycle = Math.min(s.cycle + 1, last);
        return { cycle, playing: s.playing && cycle < last };
      });
    },

    /** @param {number} speed */
    setSpeed(speed) {
      set({ speed });
    },
  };

  return {
    getState: () => state,
    /** @param {() => void} listener */
    subscribe(listener) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    actions,
  };
}

/** @typedef {ReturnType<typeof createAppStore>} AppStore */

export const appStore = createAppStore();
export const actions = appStore.actions;

/**
 * Reads part of the app state and re-renders when it changes. The selector must return a value that is
 * stable while the state is unchanged (a field, or a primitive computed from fields).
 * @template T
 * @param {(state: AppState) => T} selector
 * @returns {T}
 */
export function useAppState(selector) {
  return useSyncExternalStore(appStore.subscribe, () => selector(appStore.getState()));
}
