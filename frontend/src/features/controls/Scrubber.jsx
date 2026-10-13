import { scrubberMarks } from "../../timeline/selectors.js";
import { actions, lastCycle, useAppState } from "../../state/store.js";
import styles from "./Scrubber.module.css";

const EMPTY = /** @type {ReturnType<typeof scrubberMarks>} */ ([]);

/** The marks drawn above the track, one lane per kind of event. */
const LANES = /** @type {const} */ ([
  { key: "stall", label: "stall" },
  { key: "forward", label: "forward" },
  { key: "flush", label: "flush" },
]);

/** A slider over every cycle of the run, marked where stalls, forwarding and flushes happen. */
export default function Scrubber() {
  const timeline = useAppState((s) => s.timeline);
  const cycle = useAppState((s) => s.cycle);
  const last = useAppState(lastCycle);
  const marks = timeline ? scrubberMarks(timeline) : EMPTY;

  /** @param {number} at */
  const position = (at) => `${last === 0 ? 0 : (at / last) * 100}%`;

  return (
    <div className={styles.scrubber}>
      <div className={styles.track}>
        <div className={styles.lanes} aria-hidden="true">
          {LANES.map((lane) => (
            <div key={lane.key} className={styles.lane}>
              {marks
                .filter((mark) => mark[lane.key])
                .map((mark) => (
                  <span key={mark.cycle} className={`${styles.mark} ${styles[lane.key]}`} style={{ left: position(mark.cycle) }} />
                ))}
            </div>
          ))}
        </div>
        <input
          type="range"
          className={styles.range}
          min={0}
          max={last}
          step={1}
          value={cycle}
          disabled={last === 0}
          onChange={(event) => actions.seek(Number(event.target.value))}
          aria-label="Cycle"
          aria-valuetext={`Cycle ${cycle} of ${last}`}
        />
      </div>
      <ul className={`mono ${styles.legend}`} aria-label="Marks">
        {LANES.map((lane) => (
          <li key={lane.key}>
            <span className={`${styles.swatch} ${styles[lane.key]}`} aria-hidden="true" />
            {lane.label}
          </li>
        ))}
      </ul>
    </div>
  );
}
