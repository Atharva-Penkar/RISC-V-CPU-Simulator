import { plural } from "../../timeline/format.js";
import { actions, useAppState } from "../../state/store.js";
import styles from "./RunStatus.module.css";

/** One line on the state of the latest run: running, its size, assembly errors, or a failure with a retry. */
export default function RunStatus() {
  const status = useAppState((s) => s.status);
  const timeline = useAppState((s) => s.timeline);
  const stale = useAppState((s) => s.stale);
  const diagnostics = useAppState((s) => s.diagnostics);
  const error = useAppState((s) => s.error);

  const kept = stale && timeline ? " Showing the last run that worked." : "";

  let tone = "";
  let text;
  switch (status) {
    case "idle":
    case "running":
      text = timeline ? "Running…" : "Running the program… The server can take a few seconds to wake up.";
      break;
    case "assembly-error":
      tone = styles.error;
      text = `${plural(diagnostics.length, "assembly error")}.${kept}`;
      break;
    case "error":
      tone = styles.error;
      text = `${error}${kept}`;
      break;
    default: {
      const stats = timeline?.stats;
      text = stats ? `${plural(stats.cycles, "cycle")} · ${plural(stats.instructions, "instruction")} · CPI ${stats.cpi.toFixed(2)}` : "";
    }
  }

  return (
    <p className={`mono ${styles.status} ${tone}`} role="status">
      {status === "running" && <span className={styles.spinner} aria-hidden="true" />}
      <span>{text}</span>
      {status === "error" && (
        <button type="button" className={styles.retry} onClick={actions.retry}>
          Retry
        </button>
      )}
    </p>
  );
}
