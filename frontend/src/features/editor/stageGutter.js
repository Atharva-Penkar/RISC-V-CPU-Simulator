// The column beside the line numbers that shows which pipeline stage each line is in during the
// current cycle, and a tint on those lines. A line in a loop can be in two stages at once ("EX IF").

import { RangeSet, StateEffect, StateField } from "@codemirror/state";
import { Decoration, EditorView, gutter, GutterMarker } from "@codemirror/view";

/** @typedef {import("../../timeline/types.js").StageName} StageName */

/**
 * @typedef {object} LineStage
 * @property {StageName[]} stages
 * @property {string} color    the instruction colour (CSS value)
 * @property {boolean} stalled
 */

/** Replaces the stages shown: line number (1-based) -> stage info. */
export const setLineStages = StateEffect.define(/** @type {{ map: (value: Map<number, LineStage>) => Map<number, LineStage> }} */ ({ map: (value) => value }));

const lineStages = StateField.define({
  /** @returns {Map<number, LineStage>} */
  create: () => new Map(),
  update(value, transaction) {
    for (const effect of transaction.effects) if (effect.is(setLineStages)) return effect.value;
    // Edits make the stages refer to old line numbers until the next run arrives.
    return transaction.docChanged ? new Map() : value;
  },
});

class StageMarker extends GutterMarker {
  /** @param {LineStage} info */
  constructor(info) {
    super();
    this.info = info;
  }

  /** @param {StageMarker} other */
  eq(other) {
    return (
      other.info.color === this.info.color &&
      other.info.stalled === this.info.stalled &&
      other.info.stages.join() === this.info.stages.join()
    );
  }

  toDOM() {
    const element = document.createElement("span");
    element.className = "cm-stage" + (this.info.stalled ? " cm-stage-stalled" : "");
    element.style.setProperty("--stage-color", this.info.color);
    element.textContent = this.info.stages.join(" ");
    return element;
  }
}

const stageColumn = gutter({
  class: "cm-stage-gutter",
  markers(view) {
    const stages = view.state.field(lineStages);
    const markers = [];
    for (const [number, info] of [...stages].sort((a, b) => a[0] - b[0])) {
      if (number > view.state.doc.lines) continue;
      markers.push(new StageMarker(info).range(view.state.doc.line(number).from));
    }
    return RangeSet.of(markers);
  },
  // Keeps the column wide enough for "MEM" while empty, so it does not jump as stages appear.
  initialSpacer: () => new StageMarker({ stages: ["MEM"], color: "transparent", stalled: false }),
});

const lineTint = EditorView.decorations.compute([lineStages], (state) => {
  const stages = state.field(lineStages);
  const ranges = [...stages]
    .filter(([number]) => number <= state.doc.lines)
    .sort((a, b) => a[0] - b[0])
    .map(([number, info]) =>
      Decoration.line({ class: "cm-in-pipeline", attributes: { style: `--stage-color: ${info.color}` } }).range(
        state.doc.line(number).from,
      ),
    );
  return Decoration.set(ranges);
});

export function stageGutter() {
  return [lineStages, stageColumn, lineTint];
}
