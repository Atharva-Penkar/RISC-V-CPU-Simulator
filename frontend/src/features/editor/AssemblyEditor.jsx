import { defaultKeymap, history, historyKeymap, indentWithTab } from "@codemirror/commands";
import { EditorState } from "@codemirror/state";
import { EditorView, highlightActiveLine, keymap, lineNumbers } from "@codemirror/view";
import { useEffect, useRef } from "react";

import { instanceColor } from "../../timeline/colors.js";
import { instanceInfo, stageOccupancy } from "../../timeline/selectors.js";
import { actions, appStore, useAppState } from "../../state/store.js";
import { showDiagnostics } from "./diagnostics.js";
import { riscvLanguage } from "./riscvLanguage.js";
import { setLineStages, stageGutter } from "./stageGutter.js";
import styles from "./AssemblyEditor.module.css";

/** @typedef {import("./stageGutter.js").LineStage} LineStage */

/**
 * The stage of every source line in the pipeline during `cycle`, with the instruction's colour.
 * @param {import("../../timeline/types.js").Timeline} timeline
 * @param {number} cycle
 */
function lineStagesAt(timeline, cycle) {
  /** @type {Map<number, LineStage>} */
  const lines = new Map();
  for (const { stage, content } of stageOccupancy(timeline, cycle)) {
    if (content.kind !== "instance") continue;
    const line = instanceInfo(timeline, content.instance).line;
    if (line === null) continue;
    const entry = lines.get(line) ?? { stages: [], color: instanceColor(timeline, content.instance), stalled: false };
    entry.stages.push(stage);
    entry.stalled ||= content.stalled;
    lines.set(line, entry);
  }
  return lines;
}

const theme = EditorView.theme({
  "&": { height: "100%", fontSize: "13px", backgroundColor: "var(--paper)", border: "1px solid var(--rule)" },
  "&.cm-focused": { outline: "2px solid var(--signal)", outlineOffset: "-1px" },
  ".cm-scroller": { fontFamily: "var(--font-mono)", lineHeight: "1.6" },
  ".cm-content": { caretColor: "var(--ink)" },
  ".cm-gutters": { backgroundColor: "var(--paper)", color: "var(--ink-muted)", borderRight: "1px solid var(--rule)" },
  ".cm-activeLine": { backgroundColor: "transparent" },
  ".cm-stage-gutter .cm-gutterElement": { padding: "0 4px", display: "flex", alignItems: "center" },
  ".cm-stage": {
    fontSize: "10px",
    fontWeight: "600",
    letterSpacing: "0.05em",
    lineHeight: "16px",
    padding: "0 4px",
    color: "var(--ink)",
    border: "1px solid var(--stage-color)",
    whiteSpace: "nowrap",
  },
  ".cm-stage-stalled": { backgroundColor: "var(--stall)", borderColor: "var(--stall)", color: "#ffffff" },
  ".cm-in-pipeline": { boxShadow: "inset 3px 0 0 var(--stage-color)" },
  ".cm-lintRange-error": {
    backgroundImage: "none",
    textDecoration: "underline wavy var(--fault)",
    textUnderlineOffset: "3px",
  },
  ".cm-tooltip-lint": { fontFamily: "var(--font-mono)", fontSize: "12px" },
});

/**
 * Moves the cursor to an assembly error.
 * @param {EditorView | null} editor
 * @param {import("../../timeline/types.js").Diagnostic} diagnostic
 */
function jumpTo(editor, diagnostic) {
  if (!editor || diagnostic.line > editor.state.doc.lines) return;
  const line = editor.state.doc.line(diagnostic.line);
  const anchor = Math.min(line.from + diagnostic.column - 1, line.to);
  editor.dispatch({ selection: { anchor }, scrollIntoView: true });
  editor.focus();
}

/** The program editor: syntax highlighting, assembly errors, and the stage of each line this cycle. */
export default function AssemblyEditor() {
  const host = useRef(/** @type {HTMLDivElement | null} */ (null));
  const view = useRef(/** @type {EditorView | null} */ (null));

  const source = useAppState((s) => s.source);
  const diagnostics = useAppState((s) => s.diagnostics);
  const timeline = useAppState((s) => s.timeline);
  const stale = useAppState((s) => s.stale);
  const cycle = useAppState((s) => s.cycle);

  // Created once; the store is the source of truth for the text.
  useEffect(() => {
    const editor = new EditorView({
      parent: /** @type {HTMLDivElement} */ (host.current),
      state: EditorState.create({
        doc: appStore.getState().source,
        extensions: [
          lineNumbers(),
          stageGutter(),
          history(),
          highlightActiveLine(),
          keymap.of([...defaultKeymap, ...historyKeymap, indentWithTab]),
          riscvLanguage(),
          theme,
          EditorView.contentAttributes.of({ "aria-label": "Assembly source" }),
          EditorView.updateListener.of((update) => {
            if (update.docChanged) actions.setSource(update.state.doc.toString());
          }),
        ],
      }),
    });
    view.current = editor;
    return () => {
      editor.destroy();
      view.current = null;
    };
  }, []);

  // Text replaced from outside the editor, e.g. by the example picker.
  useEffect(() => {
    const editor = view.current;
    if (editor && editor.state.doc.toString() !== source) {
      editor.dispatch({ changes: { from: 0, to: editor.state.doc.length, insert: source } });
    }
  }, [source]);

  useEffect(() => {
    const editor = view.current;
    if (editor) editor.dispatch(showDiagnostics(editor, diagnostics));
  }, [diagnostics]);

  useEffect(() => {
    const editor = view.current;
    if (!editor) return;
    const stages = timeline && !stale ? lineStagesAt(timeline, cycle) : new Map();
    editor.dispatch({ effects: setLineStages.of(stages) });
  }, [timeline, stale, cycle]);

  return (
    <div className={styles.wrapper}>
      <div ref={host} className={styles.editor} />
      {diagnostics.length > 0 && (
        <ul className={`mono ${styles.diagnostics}`} aria-label="Assembly errors">
          {diagnostics.map((d, i) => (
            <li key={i}>
              <button type="button" onClick={() => jumpTo(view.current, d)}>
                line {d.line}:{d.column}
              </button>{" "}
              {d.message}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
