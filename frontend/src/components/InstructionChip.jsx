import { instanceColor } from "../timeline/colors.js";
import { instanceInfo } from "../timeline/selectors.js";
import styles from "./InstructionChip.module.css";

/**
 * An instance's label: its static instruction tag and disassembly, a badge for repeated executions,
 * and the instruction's colour, shared by every view so a line is easy to follow across the page.
 * @param {object} props
 * @param {import("../timeline/types.js").Timeline} props.timeline
 * @param {number} props.instance
 * @param {boolean} [props.compact]  tag and mnemonic only
 * @param {boolean} [props.wrap]     let long text wrap instead of being cut short
 */
export default function InstructionChip({ timeline, instance, compact = false, wrap = false }) {
  const info = instanceInfo(timeline, instance);
  const text = compact ? (info.instruction?.mnemonic ?? info.text) : info.text;
  return (
    <span className={`mono ${styles.chip} ${wrap ? styles.wrap : ""}`} style={{ "--chip-color": instanceColor(timeline, instance) }} title={`${info.tag} ${info.text}${info.badge ? ` (${info.badge})` : ""}`}>
      <span className={styles.tag}>{info.tag}</span>
      <span className={styles.text}>{text}</span>
      {info.badge && <span className={styles.badge}>{info.badge}</span>}
    </span>
  );
}
