import { useLayoutEffect, useRef } from "react";

import InstructionChip from "../../components/InstructionChip.jsx";
import { timingRows } from "../../timeline/selectors.js";
import { actions, useAppState } from "../../state/store.js";
import TimingCell from "./TimingCell.jsx";
import styles from "./TimingChart.module.css";

/**
 * Scrolls `container` just enough to show `target`, leaving room for the sticky header and labels.
 * Only the chart scrolls, never the page.
 * @param {HTMLElement} container
 * @param {HTMLElement} target
 * @param {number} headerHeight
 * @param {number} labelWidth
 */
function reveal(container, target, headerHeight, labelWidth) {
  const left = target.offsetLeft;
  const right = left + target.offsetWidth;
  if (left < container.scrollLeft + labelWidth) container.scrollLeft = left - labelWidth;
  else if (right > container.scrollLeft + container.clientWidth) container.scrollLeft = right - container.clientWidth;
  const top = target.offsetTop;
  const bottom = top + target.offsetHeight;
  if (top < container.scrollTop + headerHeight) container.scrollTop = top - headerHeight;
  else if (bottom > container.scrollTop + container.clientHeight) container.scrollTop = bottom - container.clientHeight;
}

/**
 * The classic pipeline diagram, in the style of the portfolio's figure: one row per executed
 * instruction, one column per cycle, ink-outlined stages and solid blue stalls. Flushed instructions are
 * dashed red; the cycle being viewed is shaded and later cycles are faded. Click a cell to go to its cycle.
 */
export default function TimingChart() {
  const timeline = useAppState((s) => s.timeline);
  const cycle = useAppState((s) => s.cycle);
  const stale = useAppState((s) => s.stale);
  const scroller = useRef(/** @type {HTMLDivElement | null} */ (null));
  const header = useRef(/** @type {HTMLDivElement | null} */ (null));

  // Keeps the current cycle, and the oldest instruction in the pipeline, in view.
  useLayoutEffect(() => {
    const container = scroller.current;
    if (!container) return;
    if (cycle === 0) {
      container.scrollTo(0, 0);
      return;
    }
    const target =
      container.querySelector(`.${styles.cell}[data-cycle="${cycle}"]`) ?? container.querySelector(`[data-cycle="${cycle}"]`);
    if (target instanceof HTMLElement) reveal(container, target, header.current?.offsetHeight ?? 0, header.current?.offsetWidth ?? 0);
  }, [cycle, timeline]);

  if (!timeline) return <p className={styles.empty}>The timing chart appears when the program has run.</p>;

  const rows = timingRows(timeline);
  const cycles = timeline.cycles.length;

  return (
    <div
      ref={scroller}
      className={`${styles.scroller} ${stale ? styles.stale : ""}`}
      onClick={(event) => {
        const cell = /** @type {HTMLElement} */ (event.target).closest("[data-cycle]");
        if (cell instanceof HTMLElement) actions.seek(Number(cell.dataset.cycle));
      }}
    >
      <div
        className={styles.chart}
        style={{
          gridTemplateColumns: `var(--label) repeat(${cycles}, var(--column))`,
          gridTemplateRows: `repeat(${rows.length + 1}, auto)`,
        }}
        role="img"
        aria-label={`Pipeline timing of ${rows.length} instructions over ${cycles} cycles`}
      >
        {cycle > 0 && <div className={styles.now} style={{ gridColumn: cycle + 1 }} aria-hidden="true" />}

        <div ref={header} className={`${styles.corner} ${styles.header}`} style={{ gridRow: 1, gridColumn: 1 }}>
          cycle
        </div>
        {Array.from({ length: cycles }, (_, i) => (
          <div
            key={i}
            className={`${styles.header} ${styles.cycle} ${i + 1 === cycle ? styles.current : ""}`}
            style={{ gridRow: 1, gridColumn: i + 2 }}
            data-cycle={i + 1}
          >
            {i + 1}
          </div>
        ))}

        {rows.map((row, index) => {
          const squashed = row.fate === "flushed" || row.fate === "discarded";
          return (
            <div key={row.instance} className={styles.rowGroup}>
              <div className={`${styles.label} ${squashed ? styles.squashedLabel : ""}`} style={{ gridRow: index + 2, gridColumn: 1 }}>
                <InstructionChip timeline={timeline} instance={row.instance} />
                {squashed && <span className={styles.fate}>{row.fate}</span>}
              </div>
              {row.cells.map((cell) => (
                <TimingCell
                  key={cell.cycle}
                  row={index + 2}
                  cycle={cell.cycle}
                  stage={cell.stage}
                  stalled={cell.stalled}
                  squashed={squashed}
                  fault={row.fate === "faulted" && cell.cycle === row.endCycle}
                  future={cell.cycle > cycle}
                />
              ))}
            </div>
          );
        })}
      </div>
    </div>
  );
}
