// Assembly errors from the backend, as CodeMirror diagnostics: a wavy underline under the offending
// token, with the message on hover.

import { setDiagnostics } from "@codemirror/lint";

/** @typedef {import("../../timeline/types.js").Diagnostic} Diagnostic */

/**
 * Converts the assembler's 1-based line/column positions into document ranges. The range covers the
 * token at the position (up to whitespace, ',', ';' or '#').
 * @param {import("@codemirror/state").Text} doc
 * @param {Diagnostic[]} diagnostics
 * @returns {import("@codemirror/lint").Diagnostic[]}
 */
export function toEditorDiagnostics(doc, diagnostics) {
  return diagnostics
    .filter((d) => d.line >= 1 && d.line <= doc.lines)
    .map((d) => {
      const line = doc.line(d.line);
      const from = Math.min(line.from + Math.max(d.column - 1, 0), line.to);
      const token = /^[^\s,;#]*/.exec(line.text.slice(from - line.from))?.[0] ?? "";
      return { from, to: from + token.length, severity: "error", message: d.message };
    });
}

/**
 * A transaction spec that shows `diagnostics` in the editor (replacing any shown before).
 * @param {import("@codemirror/view").EditorView} view
 * @param {Diagnostic[]} diagnostics
 */
export function showDiagnostics(view, diagnostics) {
  return setDiagnostics(view.state, toEditorDiagnostics(view.state.doc, diagnostics));
}
