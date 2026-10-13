import { memo } from "react";

import styles from "./TimingChart.module.css";

/**
 * One stage of one instruction in one cycle. A stalled cycle is drawn like the portfolio's pipeline
 * figure: solid signal blue, labelled "stall".
 * @param {object} props
 * @param {number} props.row      grid row (1 is the cycle header)
 * @param {number} props.cycle
 * @param {import("../../timeline/types.js").StageName} props.stage
 * @param {boolean} props.stalled
 * @param {{ cycle: number, of: number } | null} props.busy  a slow multiply/divide in EX or load/store in MEM
 * @param {boolean} props.squashed  the instruction was flushed or discarded
 * @param {boolean} props.fault     the instruction faulted here
 * @param {boolean} props.future    after the cycle being viewed
 */
function TimingCell({ row, cycle, stage, stalled, busy, squashed, fault, future }) {
  const classes = [styles.cell];
  if (stalled) classes.push(styles.stalled);
  else if (busy) classes.push(styles.busy);
  if (squashed) classes.push(styles.squashed);
  if (fault) classes.push(styles.fault);
  if (future) classes.push(styles.future);
  return (
    <div
      className={classes.join(" ")}
      style={{ gridRow: row, gridColumn: cycle + 1 }}
      data-cycle={cycle}
      title={busy ? `${stage}: cycle ${busy.cycle} of ${busy.of}` : undefined}
    >
      {stalled ? "stall" : stage}
      {busy && !stalled && <span className={styles.progress}>{busy.cycle}/{busy.of}</span>}
    </div>
  );
}

export default memo(TimingCell);
