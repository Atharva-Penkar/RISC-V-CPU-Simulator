import { useEffect, useState } from "react";

import { isAbortError } from "../../api/client.js";
import { getExample, listExamples } from "../../api/endpoints.js";
import { actions, DEFAULT_SOURCE } from "../../state/store.js";
import styles from "./ExamplePicker.module.css";

const DEMO = "demo";

/** Loads one of the backend's example programs (or the built-in demo) into the editor. */
export default function ExamplePicker() {
  const [examples, setExamples] = useState(/** @type {import("../../api/endpoints.js").ExampleSummary[]} */ ([]));
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    const controller = new AbortController();
    listExamples({ signal: controller.signal })
      .then(setExamples)
      .catch((error) => {
        // Without the list, only the demo is offered; the run status reports connection problems.
        if (!isAbortError(error)) setExamples([]);
      });
    return () => controller.abort();
  }, []);

  /** @param {string} id */
  async function load(id) {
    if (id === DEMO) {
      actions.loadProgram(DEFAULT_SOURCE);
      return;
    }
    setLoading(true);
    try {
      actions.loadProgram((await getExample(id)).source);
    } catch {
      // Leave the editor as it is; the picker stays usable.
    } finally {
      setLoading(false);
    }
  }

  return (
    <label className={`mono ${styles.picker}`}>
      <span className="visually-hidden">Load an example program</span>
      <select
        value=""
        disabled={loading}
        onChange={(event) => {
          if (event.target.value) void load(event.target.value);
        }}
      >
        <option value="" disabled>
          {loading ? "loading…" : "examples"}
        </option>
        <option value={DEMO}>demo: forwarding, a stall and flushes</option>
        {examples.map((example) => (
          <option key={example.id} value={example.id}>
            {example.id}
            {example.description ? `: ${example.description}` : ""}
          </option>
        ))}
      </select>
    </label>
  );
}
