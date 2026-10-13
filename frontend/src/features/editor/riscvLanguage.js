// Syntax highlighting for the assembler's dialect: one statement per line or per ';', an optional
// "label:" prefix, '#' comments, registers x0..x31 (the assembler takes no ABI names), decimal and 0x immediates, and
// memory operands written imm[xN] or imm(xN).

import { HighlightStyle, StreamLanguage, syntaxHighlighting } from "@codemirror/language";
import { tags } from "@lezer/highlight";

const REGISTER = /^x([0-9]|[12][0-9]|3[01])$/i;

/** @typedef {{ statementStart: boolean }} State */

const riscv = StreamLanguage.define({
  name: "riscv",
  /** @returns {State} */
  startState: () => ({ statementStart: true }),
  /**
   * @param {import("@codemirror/language").StringStream} stream
   * @param {State} state
   */
  token(stream, state) {
    if (stream.sol()) state.statementStart = true;
    if (stream.eatSpace()) return null;
    if (stream.peek() === "#") {
      stream.skipToEnd();
      return "comment";
    }
    if (stream.eat(";")) {
      state.statementStart = true;
      return "separator";
    }
    if (state.statementStart && stream.match(/^[A-Za-z_.][\w.]*\s*:/)) return "label";
    if (stream.match(/^[+-]?(0x[0-9a-f]+|\d+)\b/i)) return "number";
    if (stream.match(/^[A-Za-z_.][\w.]*/)) {
      if (state.statementStart) {
        state.statementStart = false;
        return "mnemonic";
      }
      return REGISTER.test(stream.current()) ? "register" : "labelReference";
    }
    stream.next();
    return "punctuation";
  },
  languageData: { commentTokens: { line: "#" } },
  tokenTable: {
    mnemonic: tags.keyword,
    register: tags.variableName,
    label: tags.labelName,
    labelReference: tags.labelName,
    number: tags.number,
    comment: tags.lineComment,
    separator: tags.separator,
    punctuation: tags.punctuation,
  },
});

/** Colours from the portfolio palette: ink for code, signal blue for mnemonics. */
const highlightStyle = HighlightStyle.define([
  { tag: tags.keyword, color: "var(--signal)", fontWeight: "600" },
  { tag: tags.variableName, color: "var(--ink)" },
  { tag: tags.labelName, color: "var(--inst-3)" },
  { tag: tags.number, color: "var(--inst-2)" },
  { tag: tags.lineComment, color: "var(--ink-muted)", fontStyle: "italic" },
  { tag: [tags.punctuation, tags.separator], color: "var(--ink-muted)" },
]);

export function riscvLanguage() {
  return [riscv, syntaxHighlighting(highlightStyle)];
}
