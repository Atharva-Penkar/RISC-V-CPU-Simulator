import { useMemo, useState } from "react";

import Panel from "../../components/Panel.jsx";
import { changesAt, stateAt } from "../../timeline/machineState.js";
import { writtenRegisters } from "../../timeline/selectors.js";
import { actions, useAppState } from "../../state/store.js";
import MemoryPanel from "../memory/MemoryPanel.jsx";
import RegisterPanel from "../registers/RegisterPanel.jsx";
import styles from "./MachinePanel.module.css";

const VIEWS = /** @type {const} */ (["registers", "memory"]);

const RADIXES = /** @type {const} */ ([
  { value: "hex", label: "hex" },
  { value: "signed", label: "signed" },
  { value: "unsigned", label: "unsigned" },
]);

/** Registers and data memory at the current cycle, with a choice of how values are written. */
export default function MachinePanel() {
  const timeline = useAppState((s) => s.timeline);
  const cycle = useAppState((s) => s.cycle);
  const radix = useAppState((s) => s.radix);
  const [view, setView] = useState(/** @type {(typeof VIEWS)[number]} */ ("registers"));

  const snapshot = useMemo(() => (timeline ? stateAt(timeline, cycle) : null), [timeline, cycle]);
  const changes = useMemo(() => (timeline ? changesAt(timeline, cycle) : null), [timeline, cycle]);
  const written = useMemo(() => (timeline ? writtenRegisters(timeline, cycle) : new Set()), [timeline, cycle]);
  const record = timeline?.cycles[cycle - 1] ?? null;

  const controls = (
    <>
      <div className={styles.tabs} role="tablist" aria-label="View">
        {VIEWS.map((v) => (
          <button key={v} type="button" role="tab" aria-selected={view === v} className={`mono ${styles.tab}`} onClick={() => setView(v)}>
            {v}
          </button>
        ))}
      </div>
      <label className={`mono ${styles.radix}`}>
        <span className="visually-hidden">Number format</span>
        <select value={radix} onChange={(event) => actions.setRadix(/** @type {any} */ (event.target.value))}>
          {RADIXES.map((r) => (
            <option key={r.value} value={r.value}>
              {r.label}
            </option>
          ))}
        </select>
      </label>
    </>
  );

  return (
    <Panel title="State" actions={controls}>
      {!snapshot || !changes ? (
        <p className={styles.empty}>Register and memory values appear when the program has run.</p>
      ) : view === "registers" ? (
        <RegisterPanel registers={snapshot.registers} written={written} changed={changes.registers} write={record?.registerWrite ?? null} radix={radix} />
      ) : (
        <MemoryPanel words={snapshot.memory} access={record?.memory ?? null} radix={radix} />
      )}
    </Panel>
  );
}
