// Instruction colours: every execution of a source line shares the line's colour, and repeated
// executions get an occurrence badge instead, so a loop reads as the same colours going round.

/** @typedef {import("./types.js").Timeline} Timeline */

/** Number of --inst-N colours defined in styles/tokens.css. */
export const PALETTE_SIZE = 8;

/**
 * The CSS colour of a static instruction; null (a fetch outside the program) gets a neutral colour.
 * @param {number | null} instructionId
 */
export function instructionColor(instructionId) {
  return instructionId === null ? "var(--inst-none)" : `var(--inst-${instructionId % PALETTE_SIZE})`;
}

/**
 * The CSS colour of a dynamic instance.
 * @param {Timeline} timeline
 * @param {number} instanceId
 */
export function instanceColor(timeline, instanceId) {
  return instructionColor(timeline.instances[instanceId]?.instruction ?? null);
}

/**
 * Badge for repeated executions: "" for the first, "×2" for the second...
 * @param {number} occurrence
 */
export function occurrenceBadge(occurrence) {
  return occurrence > 1 ? `×${occurrence}` : "";
}
