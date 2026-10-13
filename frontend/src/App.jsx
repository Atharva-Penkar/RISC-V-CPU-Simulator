import Panel from "./components/Panel.jsx";
import RunStatus from "./features/controls/RunStatus.jsx";
import TransportBar from "./features/controls/TransportBar.jsx";
import AppLayout from "./layout/AppLayout.jsx";
import { actions, useAppState } from "./state/store.js";
import { usePlayback } from "./state/usePlayback.js";
import { useSimulation } from "./state/useSimulation.js";
import styles from "./App.module.css";

/** A plain text area and error list until the CodeMirror editor (features/editor) replaces it. */
function SourceArea() {
  const source = useAppState((s) => s.source);
  const diagnostics = useAppState((s) => s.diagnostics);
  return (
    <div className={styles.sourceArea}>
      <textarea
        className={styles.textarea}
        value={source}
        onChange={(event) => actions.setSource(event.target.value)}
        spellCheck={false}
        wrap="off"
        aria-label="Assembly source"
      />
      {diagnostics.length > 0 && (
        <ul className={`mono ${styles.diagnostics}`}>
          {diagnostics.map((d, i) => (
            <li key={i}>
              {d.line}:{d.column} {d.message}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}

/** @param {{ children: import("react").ReactNode }} props */
function Placeholder({ children }) {
  return <p className={styles.placeholder}>{children}</p>;
}

export default function App() {
  useSimulation();
  usePlayback();

  return (
    <AppLayout
      status={<RunStatus />}
      editor={
        <Panel title="Program">
          <SourceArea />
        </Panel>
      }
      datapath={
        <Panel title="Datapath">
          <Placeholder>The five pipeline stages, with forwarding paths, stalls and flushes.</Placeholder>
        </Panel>
      }
      timing={
        <Panel title="Timing chart">
          <Placeholder>One row per instruction, one column per cycle.</Placeholder>
        </Panel>
      }
      events={
        <Panel title="What happened">
          <Placeholder>An explanation of each cycle.</Placeholder>
        </Panel>
      }
      machine={
        <Panel title="Registers and memory">
          <Placeholder>Register and memory values at the current cycle.</Placeholder>
        </Panel>
      }
      stats={
        <Panel title="Statistics">
          <Placeholder>Cycles, CPI, stalls and flushes.</Placeholder>
        </Panel>
      }
      controls={<TransportBar />}
    />
  );
}
