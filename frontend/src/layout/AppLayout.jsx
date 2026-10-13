import styles from "./AppLayout.module.css";

/**
 * The page grid. Each slot holds one region; on narrow screens the regions stack in reading order and
 * the playback controls stay pinned to the bottom of the window.
 * @param {object} props
 * @param {import("react").ReactNode} props.status    run status, at the right of the title bar
 * @param {import("react").ReactNode} props.editor
 * @param {import("react").ReactNode} props.datapath
 * @param {import("react").ReactNode} props.timing
 * @param {import("react").ReactNode} props.events
 * @param {import("react").ReactNode} props.machine   registers and memory
 * @param {import("react").ReactNode} props.stats
 * @param {import("react").ReactNode} props.controls
 */
export default function AppLayout({ status, editor, datapath, timing, events, machine, stats, controls }) {
  return (
    <div className={styles.page}>
      <header className={styles.titleBar}>
        <h1 className={styles.title}>RISC-V Pipeline Simulator</h1>
        <div className={styles.status}>{status}</div>
      </header>
      <main className={styles.grid}>
        <div className={styles.editor}>{editor}</div>
        <div className={styles.datapath}>{datapath}</div>
        <div className={styles.timing}>{timing}</div>
        <div className={styles.events}>{events}</div>
        <div className={styles.machine}>{machine}</div>
        <div className={styles.stats}>{stats}</div>
      </main>
      <footer className={styles.controls}>{controls}</footer>
    </div>
  );
}
