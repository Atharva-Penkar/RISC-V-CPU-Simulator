import { useId, useState } from "react";

import { actions, IDEAL_LATENCIES, MAX_LATENCY, TYPICAL_LATENCIES, useAppState } from "../../state/store.js";
import styles from "./PipelineSettings.module.css";

/** @typedef {import("../../timeline/types.js").Latencies} Latencies */

/** The four settings, grouped by the stage they slow down. */
const FIELDS = /** @type {const} */ ([
  { key: "multiply", label: "Multiply", hint: "MUL, MULH, MULHSU, MULHU", stage: "EX" },
  { key: "divide", label: "Divide", hint: "DIV, DIVU, REM, REMU", stage: "EX" },
  { key: "load", label: "Load", hint: "LB, LH, LW, LBU, LHU", stage: "MEM" },
  { key: "store", label: "Store", hint: "SB, SH, SW", stage: "MEM" },
]);

/**
 * @param {Latencies} a
 * @param {Latencies} b
 */
function same(a, b) {
  return a.multiply === b.multiply && a.divide === b.divide && a.load === b.load && a.store === b.store;
}

/**
 * One latency: a number field that only updates the store with whole numbers from 1 to MAX_LATENCY, so
 * typing "12" does not run the program with 1 first and does not reject the half-typed value.
 * @param {{ field: (typeof FIELDS)[number], value: number }} props
 */
function LatencyField({ field, value }) {
  const id = useId();
  const [draft, setDraft] = useState(/** @type {string | null} */ (null));
  const text = draft ?? String(value);
  const number = Number(text);
  const valid = Number.isInteger(number) && number >= 1 && number <= MAX_LATENCY;

  return (
    <div className={styles.field}>
      <label htmlFor={id} className={styles.label}>
        {field.label}
        <span className={`mono ${styles.hint}`}>
          {field.hint} · {field.stage}
        </span>
      </label>
      <div className={styles.input}>
        <button type="button" aria-label={`Fewer cycles for ${field.label.toLowerCase()}`} disabled={value <= 1} onClick={() => actions.setLatencies({ [field.key]: value - 1 })}>
          −
        </button>
        <input
          id={id}
          type="number"
          inputMode="numeric"
          min={1}
          max={MAX_LATENCY}
          value={text}
          aria-invalid={!valid}
          onChange={(event) => {
            setDraft(event.target.value);
            const next = Number(event.target.value);
            if (Number.isInteger(next) && next >= 1 && next <= MAX_LATENCY) actions.setLatencies({ [field.key]: next });
          }}
          onBlur={() => setDraft(null)}
        />
        <button type="button" aria-label={`More cycles for ${field.label.toLowerCase()}`} disabled={value >= MAX_LATENCY} onClick={() => actions.setLatencies({ [field.key]: value + 1 })}>
          +
        </button>
        <span className={`mono ${styles.unit}`}>{value === 1 ? "cycle" : "cycles"}</span>
      </div>
    </div>
  );
}

/**
 * The pipeline menu: forwarding on or off, and how many cycles multiplies and divides spend in EX and loads
 * and stores in MEM. Opens as a popover (light dismiss: Escape or a click outside closes it); every change
 * runs the program again, so the statistics show what each piece of hardware is worth.
 */
export default function PipelineSettings() {
  const latencies = useAppState((s) => s.latencies);
  const forwarding = useAppState((s) => s.forwarding);
  const popoverId = useId();
  const titleId = useId();
  const forwardingId = useId();
  const ideal = same(latencies, IDEAL_LATENCIES);
  const standard = ideal && forwarding;

  return (
    <>
      <button
        type="button"
        className={`mono ${styles.trigger}`}
        popoverTarget={popoverId}
        aria-label={`Pipeline options: ${standard ? "standard" : "changed"}`}
      >
        pipeline{!standard && <span className={styles.dot} aria-hidden="true" />}
      </button>
      <div id={popoverId} popover="auto" className={styles.popover} role="dialog" aria-labelledby={titleId}>
        <div className={styles.header}>
          <h2 id={titleId} className={`mono ${styles.title}`}>
            Pipeline options
          </h2>
          <button type="button" className={`mono ${styles.close}`} popoverTarget={popoverId} popoverTargetAction="hide" aria-label="Close">
            ✕
          </button>
        </div>
        <div className={styles.switchRow}>
          <label htmlFor={forwardingId} className={styles.label}>
            Forwarding
            <span className={styles.hint}>
              {forwarding
                ? "Results go straight from EX/MEM and MEM/WB into EX. Only a load followed by its use stalls."
                : "Off: an instruction waits in ID until every value it reads has been written back. Compare the cycles and CPI."}
            </span>
          </label>
          <input
            id={forwardingId}
            type="checkbox"
            role="switch"
            className={styles.switch}
            checked={forwarding}
            onChange={(event) => actions.setForwarding(event.target.checked)}
          />
        </div>
        <h3 className={`mono ${styles.subtitle}`}>Cycles per operation</h3>
        <p className={styles.intro}>
          A multiply or divide stays in EX, and a load or store in MEM, for this many cycles. Until it is done, the
          stages behind it hold their instructions and bubbles move on, so the cost shows up in the timing chart and the
          CPI.
        </p>
        <div className={styles.fields}>
          {FIELDS.map((field) => (
            <LatencyField key={field.key} field={field} value={latencies[field.key]} />
          ))}
        </div>
        <div className={styles.presets}>
          <span className="mono">presets</span>
          <button type="button" aria-pressed={ideal} onClick={() => actions.setLatencies(IDEAL_LATENCIES)}>
            ideal: all 1
          </button>
          <button type="button" aria-pressed={same(latencies, TYPICAL_LATENCIES)} onClick={() => actions.setLatencies(TYPICAL_LATENCIES)}>
            typical: {TYPICAL_LATENCIES.multiply} / {TYPICAL_LATENCIES.divide} / {TYPICAL_LATENCIES.load} / {TYPICAL_LATENCIES.store}
          </button>
        </div>
      </div>
    </>
  );
}
