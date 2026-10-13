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
 * @param {boolean} props.squashed  the instruction was flushed or discarded
 * @param {boolean} props.fault     the instruction faulted here
 * @param {boolean} props.future    after the cycle being viewed
 */
function TimingCell({ row, cycle, stage, stalled, squashed, fault, future }) {
  const classes = [styles.cell];
  if (stalled) classes.push(styles.stalled);
  if (squashed) classes.push(styles.squashed);
  if (fault) classes.push(styles.fault);
  if (future) classes.push(styles.future);
  return (
    <div className={classes.join(" ")} style={{ gridRow: row, gridColumn: cycle + 1 }} data-cycle={cycle}>
      {stalled ? "stall" : stage}
    </div>
  );
}

export default memo(TimingCell);
