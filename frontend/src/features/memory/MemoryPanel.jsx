import { formatValue, hex } from "../../timeline/format.js";
import styles from "./MemoryPanel.module.css";

const WORDS_PER_ROW = 4;

/**
 * Data memory at the current cycle, as 32-bit little-endian words, four to a row. The word stored to
 * this cycle is highlighted; the word a load reads this cycle is outlined.
 * @param {object} props
 * @param {number[]} props.words
 * @param {import("../../timeline/types.js").MemoryAccess | null} props.access  this cycle's access, if any
 * @param {import("../../timeline/format.js").Radix} props.radix
 */
export default function MemoryPanel({ words, access, radix }) {
  const accessed = access ? access.address & ~3 : -1;
  const rows = [];
  for (let start = 0; start < words.length; start += WORDS_PER_ROW) rows.push(start);
  return (
    <table className={`mono ${styles.memory}`}>
      <caption className={styles.caption}>
        {words.length * 4} bytes of data memory, little-endian words
      </caption>
      <thead>
        <tr>
          <th scope="col">address</th>
          {Array.from({ length: WORDS_PER_ROW }, (_, i) => (
            <th key={i} scope="col">+{i * 4}</th>
          ))}
        </tr>
      </thead>
      <tbody>
        {rows.map((start) => (
          <tr key={start}>
            <th scope="row">{hex(start * 4, 3)}</th>
            {words.slice(start, start + WORDS_PER_ROW).map((value, i) => {
              const address = (start + i) * 4;
              const classes = [styles.word];
              if (value === 0) classes.push(styles.zero);
              if (address === accessed) classes.push(access?.access === "store" ? styles.stored : styles.loaded);
              return (
                <td key={address} className={classes.join(" ")} title={`${hex(address)}: ${hex(value)}`}>
                  {formatValue(value, radix)}
                </td>
              );
            })}
          </tr>
        ))}
      </tbody>
    </table>
  );
}
