# RISC-V Pipeline Simulator: frontend

A React website that shows how the RISC-V assembler and the 5-stage pipelined CPU work, one clock cycle at a time.
The browser sends a program to the backend API (`../backend`), which assembles and runs it on the C++ simulator and
returns a timeline of every cycle ([format](../docs/timeline-format.md)). The page is a pure function of that timeline
and the current cycle: stepping, scrubbing and playback never re-run the simulator.

> Status: scaffold only. The folders and files below exist as empty placeholders; the application is not written yet.

## Planned layout

```
┌──────────────────────────────┬─────────────────────────────────────────────┐
│ Editor (validated live)      │  CPU datapath                               │
│  1 ▶EX  addi x1, x0, 5       │  [IF]→║→[ID]→║→[EX]→║→[MEM]→║→[WB]         │
│  2 ▶ID  add  x2, x1, x1      │        forwarding arcs, stall/flush marks   │
│  3 ▶IF  sw   x2, 0(x0)       ├─────────────────────────────────────────────┤
│  …                           │  Timing chart (rows = instrs, cols = cycles)│
├──────────────────────────────┼──────────────────────┬──────────────────────┤
│ Event log / explanations     │ Registers / Memory   │ Stats: CPI, stalls…  │
├──────────────────────────────┴──────────────────────┴──────────────────────┤
│ ⏮ ◀  ▶ ⏭  ▷ play  speed   ━━━━━━●━━━━━━━━━━━  cycle 7/30  ▲stall ◆fwd ✕flush│
└────────────────────────────────────────────────────────────────────────────┘
```

The visual style follows the [portfolio site](https://atharva-penkar.github.io/portfolio/): paper background, ink
rules, one blue signal colour, IBM Plex Sans and Mono.

## Running

Requires Node.js 20.19 or newer.

```sh
cd frontend
npm install
npm run dev       # development server
npm run build     # production build in dist/
npm run preview   # serve the production build
npm run lint      # ESLint
```

The backend must be running for the app to work: see [backend/README.md](../backend/README.md) (`npm start` serves
the API on http://localhost:3001).

## Structure

```
src/
  main.jsx, App.jsx    entry point; App wires the layout and features together
  api/                 talking to the backend, nothing else
    client.js          fetch wrapper, request cancellation, error responses
    endpoints.js       simulate, assemble, listExamples, getExample, health
  timeline/            domain logic in plain JavaScript, no React
    types.js           JSDoc types for the timeline JSON
    parse.js           checks the document format and version
    machineState.js    registers and memory at cycle N
    selectors.js       stage occupancy, instance labels, scrubber events
    explain.js         English sentences from forwarding, stall and flush records
    colors.js          instruction colours
    format.js          hex, signed decimal, register names
  state/               app state: source text, timeline, current cycle
    store.js           the store (React's useSyncExternalStore)
    useSimulation.js   debounced assemble and simulate, cancelling stale requests
    usePlayback.js     play/pause timer and speed
  features/            one folder per screen region
    editor/            assembly editor, syntax, diagnostics, pipeline-stage gutter
    timing-chart/      rows = instructions, columns = cycles
    controls/          transport bar (with keyboard shortcuts) and scrubber
    registers/  memory/  event-log/  stats/  examples/
  components/          shared UI: Panel, InstructionChip
  layout/              the page grid
  styles/              tokens.css (colours, spacing, instruction palette), global.css
scripts/
  generate-fixtures.mjs  runs ../build/rvsim --json on ../tests/programs/*.s
tests/
  fixtures/            timeline JSON produced by the script above
  unit/                unit tests for timeline/ with Node's built-in test runner
```

`jsconfig.json` enables JSDoc type checking in the editor; `.env.example` lists the settings for production builds.
