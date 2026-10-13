// Advances the cycle while playing, at the chosen speed. Use once, at the top of the app.

import { useEffect } from "react";

import { actions, useAppState } from "./store.js";

export function usePlayback() {
  const playing = useAppState((s) => s.playing);
  const speed = useAppState((s) => s.speed);

  useEffect(() => {
    if (!playing) return;
    const timer = setInterval(actions.tick, 1000 / speed);
    return () => clearInterval(timer);
  }, [playing, speed]);
}
