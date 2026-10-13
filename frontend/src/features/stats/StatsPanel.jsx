import { explainOutcome } from "../../timeline/explain.js";
import { useAppState } from "../../state/store.js";
import styles from "./StatsPanel.module.css";

/** Totals for the whole run, and how it ended. */
export default function StatsPanel() {
  const timeline = useAppState((s) => s.timeline);
  if (!timeline) return <p className={styles.quiet}>Statistics appear when the program has run.</p>;

  const { stats, latencies } = timeline;
  const items = [
    { label: "cycles", value: stats.cycles },
    { label: "instructions", value: stats.instructions, hint: "retired: completed WB" },
    { label: "CPI", value: stats.cpi.toFixed(2), hint: "cycles per instruction; 1.00 is a full pipeline" },
    { label: "stalls", value: stats.stalls, hint: "data hazard bubbles: load-use, or any dependency without forwarding" },
    { label: "EX waits", value: stats.executeStalls, hint: "cycles a slow multiply or divide held the pipeline" },
    { label: "MEM waits", value: stats.memoryStalls, hint: "cycles a slow load or store held the pipeline" },
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
      <p className={`mono ${styles.latencies}`}>
        forwarding {timeline.forwarding ? "on" : "off"} · cycles per operation: multiply {latencies.multiply} · divide{" "}
        {latencies.divide} · load {latencies.load} · store {latencies.store}
      </p>
    </div>
  );
}
