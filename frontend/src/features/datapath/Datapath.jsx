import InstructionChip from "../../components/InstructionChip.jsx";
import { hex, registerName } from "../../timeline/format.js";
import { stageOccupancy } from "../../timeline/selectors.js";
import { useAppState } from "../../state/store.js";
import styles from "./Datapath.module.css";

const LATCHES = ["IF/ID", "ID/EX", "EX/MEM", "MEM/WB"];

const NEXT_PC = {
  "pc+4": "PC + 4",
  "branch-target": "branch target",
  "jump-target": "jump target",
  hold: "held",
  stopped: "stopped",
};

/**
 * The five stages and what each works on this cycle, with stalls, flushes and forwarding marked.
 * A simplified stand-in for the full datapath diagram (an SVG of the CPU), which will replace it.
 */
export default function Datapath() {
  const timeline = useAppState((s) => s.timeline);
  const cycle = useAppState((s) => s.cycle);

  if (!timeline) return <p className={styles.quiet}>The pipeline appears when the program has run.</p>;

  const record = timeline.cycles[cycle - 1] ?? null;
  const squashed = new Set(record?.flush?.squashed ?? []);
  const stages = stageOccupancy(timeline, cycle);

  return (
    <div className={styles.datapath}>
      <ol className={styles.stages} aria-label={cycle === 0 ? "Pipeline before the first cycle" : `Pipeline in cycle ${cycle}`}>
        {stages.map(({ stage, content }, index) => {
          const classes = [styles.stage];
          let body;
          let note = null;
          if (content.kind === "instance") {
            if (content.busy) {
              classes.push(styles.busy);
              note = `busy ${content.busy.cycle}/${content.busy.of}`;
            }
            if (content.stalled) {
              classes.push(styles.stalled);
              note = "stalled";
            }
            if (squashed.has(content.instance)) {
              classes.push(styles.flushed);
              note = "flushed";
            }
            body = <InstructionChip timeline={timeline} instance={content.instance} wrap />;
          } else if (content.kind === "bubble") {
            classes.push(styles.bubble);
            body = <span className="mono">bubble</span>;
            note = content.reason === "stall" ? "from a stall" : "from a flush";
          } else {
            classes.push(styles.idle);
            body = <span className="mono">—</span>;
          }
          return (
            <li key={stage} className={styles.slot}>
              <div className={classes.join(" ")}>
                <span className={`mono ${styles.name}`}>{stage}</span>
                <span className={styles.body}>{body}</span>
                {note && <span className={`mono ${styles.note}`}>{note}</span>}
              </div>
              {index < LATCHES.length && <span className={styles.latch} title={`${LATCHES[index]} pipeline register`} />}
            </li>
          );
        })}
      </ol>

      {record && (
        <ul className={`mono ${styles.signals}`}>
          <li>
            PC {hex(record.pc)} → next {hex(record.nextPc.value)} ({NEXT_PC[record.nextPc.source]})
          </li>
          {record.forwarding.map((f, i) => (
            <li key={i} className={styles.forward}>
              forward {registerName(f.register)} = {hex(f.value)} from {f.from} to {f.operand}
            </li>
          ))}
          {record.stall && (
            <li className={styles.stallText}>
              stall: {registerName(record.stall.register)} not loaded yet; bubble into EX
            </li>
          )}
          {record.busy.map((b) => (
            <li key={b.stage} className={styles.stallText}>
              {b.stage} busy: cycle {b.cycle} of {b.of}
              {b.cycle < b.of ? "; the stages behind it wait" : "; done this cycle"}
            </li>
          ))}
          {record.flush && <li className={styles.flushText}>flush: fetch redirected to {hex(record.flush.target)}</li>}
        </ul>
      )}
    </div>
  );
}
