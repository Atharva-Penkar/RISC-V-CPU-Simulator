import { abiName, formatValue, hex, registerName, signed } from "../../timeline/format.js";
import styles from "./RegisterPanel.module.css";

/**
 * The 32 registers at the current cycle. The register written this cycle is highlighted, with its old
 * value on hover; registers holding zero are muted so the live ones stand out.
 * @param {object} props
 * @param {number[]} props.registers
 * @param {Set<number>} props.changed
 * @param {import("../../timeline/types.js").RegisterWrite | null} props.write  this cycle's write, if any
 * @param {import("../../timeline/format.js").Radix} props.radix
 */
export default function RegisterPanel({ registers, changed, write, radix }) {
  return (
    <ul className={`mono ${styles.registers}`} aria-label="Registers">
      {registers.map((value, index) => {
        const isChanged = changed.has(index);
        const title = `${registerName(index)} (${abiName(index)}) = ${hex(value)} = ${signed(value)}` +
          (isChanged && write ? `, was ${hex(write.previous)}` : "");
        return (
          <li key={index} className={`${styles.register} ${isChanged ? styles.changed : ""} ${value === 0 ? styles.zero : ""}`} title={title}>
            <span className={styles.name}>{registerName(index)}</span>
            <span className={styles.abi}>{abiName(index)}</span>
            <span className={styles.value}>{formatValue(value, radix)}</span>
          </li>
        );
      })}
    </ul>
  );
}
