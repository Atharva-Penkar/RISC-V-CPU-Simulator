import { explainCycle } from "../../timeline/explain.js";
import { useAppState } from "../../state/store.js";
import styles from "./EventLog.module.css";

/** Labels for each kind of sentence, in the colours used across the page. */
const KINDS = {
  "register-write": { label: "WB", className: styles.write },
  memory: { label: "MEM", className: styles.memory },
  forward: { label: "fwd", className: styles.forward },
  branch: { label: "EX", className: styles.branch },
  stall: { label: "stall", className: styles.stall },
  flush: { label: "flush", className: styles.flush },
  outcome: { label: "end", className: styles.outcome },
};

/** What happened in the current cycle, in plain English, generated from the timeline. */
export default function EventLog() {
  const timeline = useAppState((s) => s.timeline);
  const cycle = useAppState((s) => s.cycle);

  if (!timeline) return <p className={styles.quiet}>Explanations appear when the program has run.</p>;
  if (cycle === 0) {
    return (
      <p className={styles.quiet}>
        Before the first cycle: the pipeline is empty and every register is zero. Press play, or → to step one cycle.
      </p>
    );
  }

  const events = explainCycle(timeline, cycle);
  return (
    <div className={styles.log}>
      <h3 className={`mono ${styles.heading}`}>Cycle {cycle}</h3>
      {events.length === 0 ? (
        <p className={styles.quiet}>Instructions move one stage along; nothing else happens this cycle.</p>
      ) : (
        <ol className={styles.events}>
          {events.map((event, i) => {
            const kind = KINDS[event.kind];
            return (
              <li key={i} className={styles.event}>
                <span className={`mono ${styles.kind} ${kind.className}`}>{kind.label}</span>
                <span>{event.text}</span>
              </li>
            );
          })}
        </ol>
      )}
    </div>
  );
}
