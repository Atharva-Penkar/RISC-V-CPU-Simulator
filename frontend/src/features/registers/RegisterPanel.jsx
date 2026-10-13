import { abiName, formatValue, hex, registerName, signed } from "../../timeline/format.js";
import styles from "./RegisterPanel.module.css";

/**
 * The 32 registers at the current cycle. Registers the program has written so far are marked, and the
 * others muted, so the few that matter stand out; the register written this cycle is highlighted, with its
 * old value on hover.
 * @param {object} props
 * @param {number[]} props.registers
 * @param {Set<number>} props.written  registers written at least once by this cycle
 * @param {Set<number>} props.changed
 * @param {import("../../timeline/types.js").RegisterWrite | null} props.write  this cycle's write, if any
 * @param {import("../../timeline/format.js").Radix} props.radix
 */
export default function RegisterPanel({ registers, written, changed, write, radix }) {
  return (
    <div>
      <p className={`mono ${styles.summary}`}>
        <span className={styles.marker} aria-hidden="true" /> written so far: {written.size === 0 ? "none" : `${written.size} of 31`}
      </p>
      <ul className={`mono ${styles.registers}`} aria-label="Registers">
      {registers.map((value, index) => {
        const isChanged = changed.has(index);
        const isWritten = written.has(index);
        const title = `${registerName(index)} (${abiName(index)}) = ${hex(value)} = ${signed(value)}` +
          (isWritten ? "" : ", not written yet") +
          (isChanged && write ? `, was ${hex(write.previous)}` : "");
        const classes = [styles.register, isWritten ? styles.written : styles.unwritten, isChanged ? styles.changed : ""];
        return (
          <li key={index} className={classes.join(" ")} title={title}>
            <span className={styles.name}>
              {isWritten && <span className={styles.marker} aria-label="written" />}
              {registerName(index)}
            </span>
            <span className={styles.abi}>{abiName(index)}</span>
            <span className={styles.value}>{formatValue(value, radix)}</span>
          </li>
        );
      })}
      </ul>
    </div>
  );
}
