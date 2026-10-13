# RISC-V Pipeline Simulator: frontend

A React website that shows how the RISC-V assembler and the 5-stage pipelined CPU work, one clock cycle at a time.
The browser sends a program to the backend API (`../backend`), which assembles and runs it on the C++ simulator and
returns a timeline of every cycle ([format](../docs/timeline-format.md)). The page is a pure function of that timeline
and the current cycle: stepping, scrubbing and playback never re-run the simulator.

The site has two pages: a landing page (`index.html`) with the guide, the instruction reference and the timing
model, and the simulator (`simulator/index.html`). The landing page's button opens the simulator.

> Status: every panel works. The pipeline panel is a simplified five-stage view (features/datapath) until the full
> datapath diagram is drawn as an SVG.

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
npm test          # unit tests (Node's built-in test runner)
```

The backend must be running for the app to work: see [backend/README.md](../backend/README.md) (`npm start` serves
the API on http://localhost:3001).

## Deploying

The site is published to GitHub Pages at https://atharva-penkar.github.io/RISC-V-CPU-Simulator/ by
`.github/workflows/pages.yml`, on every push to `main` that changes `frontend/` (or by hand from the Actions tab).
The workflow runs the tests, builds with the backend URL baked in, and deploys `dist/`.

One-time setup:

1. **Settings → Pages → Build and deployment → Source: GitHub Actions.**
2. Optional: **Settings → Secrets and variables → Actions → Variables**, add `VITE_API_BASE_URL` to point the site at
   a different backend. Without it the workflow uses the Cloud Run service in asia-south1.
3. Optional: lock the backend to the site by setting `CORS_ORIGIN=https://atharva-penkar.github.io` on the Cloud
   Run service (keep `*` while developing locally against it).

The production build uses the base path `/RISC-V-CPU-Simulator/` (see `vite.config.js`); rename it there if the
repository is renamed.

## Structure

```
src/
  main.jsx, App.jsx    simulator page entry point; App wires the layout and features together
  landing/             the landing page: guide, instruction reference, timing model (main.jsx, Landing.jsx)
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
    datapath/          the five stages this cycle (simplified until the datapath SVG)
    settings/          the pipeline menu: forwarding on or off, cycles for multiply, divide, load and store
    machine/           registers and memory panel with tabs and number format
    registers/  memory/  event-log/  stats/  examples/
  components/          shared UI: Panel, InstructionChip
  layout/              the page grid
  styles/              tokens.css (colours, spacing, instruction palette), global.css, base.js (both pages import it)
scripts/
  generate-fixtures.mjs  runs ../build/rvsim --json on ../tests/programs/*.s
tests/
  fixtures/            timeline JSON produced by the script above
  unit/                unit tests for timeline/ with Node's built-in test runner
```

`jsconfig.json` enables JSDoc type checking in the editor; `.env.example` lists the settings for production builds.
