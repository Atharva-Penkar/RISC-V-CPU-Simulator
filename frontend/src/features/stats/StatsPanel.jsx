import { explainOutcome } from "../../timeline/explain.js";
import { useAppState } from "../../state/store.js";
import styles from "./StatsPanel.module.css";

/** Totals for the whole run, and how it ended. */
export default function StatsPanel() {
  const timeline = useAppState((s) => s.timeline);
  if (!timeline) return <p className={styles.quiet}>Statistics appear when the program has run.</p>;

  const { stats } = timeline;
  const items = [
    { label: "cycles", value: stats.cycles },
    { label: "instructions", value: stats.instructions, hint: "retired: completed WB" },
    { label: "CPI", value: stats.cpi.toFixed(2), hint: "cycles per instruction; 1.00 is a full pipeline" },
    { label: "stalls", value: stats.stalls, hint: "load-use bubbles" },
    { label: "flushed", value: stats.flushes, hint: "instructions fetched on a wrong path" },
    { label: "redirects", value: stats.redirects, hint: "taken branches and jumps" },
  ];

  return (
    <div className={styles.stats}>
      <dl className={styles.grid}>
        {items.map((item) => (
          <div key={item.label} className={styles.item} title={item.hint}>
            <dt className="mono">{item.label}</dt>
            <dd>{item.value}</dd>
          </div>
        ))}
      </dl>
      <p className={`${styles.outcome} ${timeline.outcome.status === "faulted" ? styles.fault : ""}`}>{explainOutcome(timeline)}</p>
    </div>
  );
}
