// Runs the program whenever it changes: waits for typing to pause, then asks the backend to simulate it.
// A newer edit cancels the request in flight, so only the latest source can update the page.

import { useEffect } from "react";

import { isAbortError } from "../api/client.js";
import { assemblyDiagnostics, simulate } from "../api/endpoints.js";
import { actions, appStore, useAppState } from "./store.js";

/** How long typing must pause before the program runs, in milliseconds. */
export const DEBOUNCE_MS = 400;

/** Keeps the timeline in the store up to date with the source. Use once, at the top of the app. */
export function useSimulation() {
  const source = useAppState((s) => s.source);
  const maxCycles = useAppState((s) => s.maxCycles);
  const latencies = useAppState((s) => s.latencies);
  const forwarding = useAppState((s) => s.forwarding);
  const runId = useAppState((s) => s.runId);

  useEffect(() => {
    const controller = new AbortController();
    // The first run, and retries after an error, start at once.
    const { timeline, status } = appStore.getState();
    const delay = timeline === null || status === "error" ? 0 : DEBOUNCE_MS;

    const timer = setTimeout(async () => {
      actions.runStarted();
      try {
        actions.runSucceeded(await simulate(source, { maxCycles, latencies, forwarding, signal: controller.signal }));
      } catch (error) {
        if (isAbortError(error)) return;
        const diagnostics = assemblyDiagnostics(error);
        actions.runFailed(
          diagnostics ? { diagnostics } : { message: error instanceof Error ? error.message : String(error) },
        );
      }
    }, delay);

    return () => {
      clearTimeout(timer);
      controller.abort();
    };
  }, [source, maxCycles, latencies, forwarding, runId]);
}
