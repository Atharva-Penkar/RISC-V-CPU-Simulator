import { useEffect } from "react";

import { actions, lastCycle, SPEEDS, useAppState } from "../../state/store.js";
import Scrubber from "./Scrubber.jsx";
import styles from "./TransportBar.module.css";

/** Shapes for the buttons, drawn on a 16×16 grid. */
const ICONS = {
  first: "M3 3h2v10H3zM13 3v10L6 8z",
  previous: "M12 3v10L5 8zM3 3h2v10H3z",
  play: "M4 2.5v11L13.5 8z",
  pause: "M4 3h3v10H4zM9 3h3v10H9z",
  next: "M4 3v10l7-5zM11 3h2v10h-2z",
  last: "M3 3v10l7-5zM11 3h2v10h-2z",
};

/** @param {{ shape: keyof typeof ICONS }} props */
function Icon({ shape }) {
  return (
    <svg viewBox="0 0 16 16" width="16" height="16" aria-hidden="true" focusable="false">
      <path d={ICONS[shape]} fill="currentColor" />
    </svg>
  );
}

/**
 * Whether a key press belongs to something else on the page: typing in a field or the editor, or a
 * shortcut with a modifier.
 * @param {KeyboardEvent} event
 */
function belongsElsewhere(event) {
  if (event.defaultPrevented || event.ctrlKey || event.metaKey || event.altKey) return true;
  const target = /** @type {HTMLElement | null} */ (event.target);
  if (!target) return false;
  if (target.isContentEditable) return true;
  return ["INPUT", "TEXTAREA", "SELECT"].includes(target.tagName);
}

/**
 * Keyboard shortcuts for playback, active anywhere outside text fields:
 * Space or K play/pause, ← → (or J L) step a cycle, with Shift ten cycles, Home and End jump to the ends.
 * @param {KeyboardEvent} event
 */
function handleShortcut(event) {
  if (belongsElsewhere(event)) return;
  const target = /** @type {HTMLElement | null} */ (event.target);
  const stride = event.shiftKey ? 10 : 1;
  switch (event.key) {
    case " ":
      // A focused button already responds to Space.
      if (target?.tagName === "BUTTON") return;
      actions.togglePlay();
      break;
    case "k":
    case "K":
      actions.togglePlay();
      break;
    case "ArrowLeft":
    case "j":
    case "J":
      actions.step(-stride);
      break;
    case "ArrowRight":
    case "l":
    case "L":
      actions.step(stride);
      break;
    case "Home":
      actions.step(-Infinity);
      break;
    case "End":
      actions.step(Infinity);
      break;
    default:
      return;
  }
  event.preventDefault();
}

/** Playback controls, the scrubber and the cycle counter. */
export default function TransportBar() {
  const cycle = useAppState((s) => s.cycle);
  const last = useAppState(lastCycle);
  const playing = useAppState((s) => s.playing);
  const speed = useAppState((s) => s.speed);

  useEffect(() => {
    window.addEventListener("keydown", handleShortcut);
    return () => window.removeEventListener("keydown", handleShortcut);
  }, []);

  const empty = last === 0;
  const atStart = cycle === 0;
  const atEnd = cycle === last;

  return (
    <div className={styles.bar}>
      <div className={styles.buttons} role="group" aria-label="Playback">
        <button type="button" className={styles.button} onClick={() => actions.step(-Infinity)} disabled={empty || atStart} aria-label="First cycle" title="First cycle (Home)">
          <Icon shape="first" />
        </button>
        <button type="button" className={styles.button} onClick={() => actions.step(-1)} disabled={empty || atStart} aria-label="Previous cycle" title="Previous cycle (←, Shift for 10)">
          <Icon shape="previous" />
        </button>
        <button
          type="button"
          className={`${styles.button} ${styles.play}`}
          onClick={actions.togglePlay}
          disabled={empty}
          aria-label={playing ? "Pause" : "Play"}
          title={`${playing ? "Pause" : "Play"} (Space)`}
        >
          <Icon shape={playing ? "pause" : "play"} />
        </button>
        <button type="button" className={styles.button} onClick={() => actions.step(1)} disabled={empty || atEnd} aria-label="Next cycle" title="Next cycle (→, Shift for 10)">
          <Icon shape="next" />
        </button>
        <button type="button" className={styles.button} onClick={() => actions.step(Infinity)} disabled={empty || atEnd} aria-label="Last cycle" title="Last cycle (End)">
          <Icon shape="last" />
        </button>
      </div>

      <label className={`mono ${styles.speed}`}>
        <span>Speed</span>
        <select value={speed} onChange={(event) => actions.setSpeed(Number(event.target.value))}>
          {SPEEDS.map((s) => (
            <option key={s} value={s}>
              {s}/s
            </option>
          ))}
        </select>
      </label>

      <Scrubber />

      <output className={`mono ${styles.counter}`} aria-live="off">
        Cycle <strong>{cycle}</strong> / {last}
      </output>
    </div>
  );
}
