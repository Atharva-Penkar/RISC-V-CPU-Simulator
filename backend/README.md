# RISC-V simulator API

A small Node.js (Express) server that accepts RISC-V assembly from a frontend, runs it on the C++ simulator and
returns the recorded pipeline timeline as JSON. The assembler and the CPU run only in C++ (the `rvasm` and `rvsim`
executables); this server validates requests, runs those executables with limits, and passes their JSON on.

## Running

Requires Node.js 20+, plus CMake and a C++20 compiler to build the simulator.

```sh
cd backend
npm install
npm run build:simulator     # builds ../build/rvsim and ../build/rvasm (Release)
npm start                   # http://localhost:3001
npm test
```

`npm run dev` restarts the server when a file changes.

## Configuration

Environment variables, all optional:

| Variable | Default | Meaning |
| --- | --- | --- |
| `PORT` | `3001` | Port to listen on. |
| `RVSIM_PATH`, `RVASM_PATH` | `../build/rvsim`, `../build/rvasm` | Simulator executables. |
| `EXAMPLES_DIR` | `../tests/programs` | Example programs (`*.s`). |
| `CORS_ORIGIN` | `*` | `Access-Control-Allow-Origin` value, e.g. `http://localhost:5173`. |
| `MAX_SOURCE_BYTES` | `65536` | Largest accepted program. |
| `DEFAULT_MAX_CYCLES` / `MAX_CYCLES_LIMIT` | `1000` / `10000` | Cycle limit when none is given, and the largest allowed. |
| `RUN_TIMEOUT_MS` | `10000` | A run taking longer is killed. |
| `MAX_OUTPUT_BYTES` | `33554432` | A run printing more JSON is killed. |
| `MAX_CONCURRENT_RUNS` | `4` | Simulator processes at once; further requests wait. |

## Endpoints

### `POST /api/simulate`

```json
{ "source": "addi x1, x0, 5\nadd x2, x1, x1\nebreak", "maxCycles": 500 }
```

`maxCycles` is optional. The answer (200) is the timeline document described in
[docs/timeline-format.md](../docs/timeline-format.md): the program with its source map, every dynamic instruction,
and per cycle the stage occupancy, forwarding, stalls, flushes, EX datapath values, memory and register writes,
then the outcome, statistics and final state. A program that halts, faults or reaches the cycle limit is still a 200;
`outcome.status` says which.

### `POST /api/assemble`

`{ "source": "..." }`. The answer (200) is the program document: every instruction with its encoding, disassembly,
fields, control signals and source location.

### `GET /api/examples`, `GET /api/examples/:id`

The programs in `tests/programs`: `{ "examples": [{ "id", "description" }] }`, and `{ "id", "description", "source" }`
for one.

### `GET /api/health`

`{ "status": "ok" | "degraded", "simulator": { "rvsim": true, "rvasm": true }, "limits": { ... } }`.

### Errors

Every error is `{ "error": { "code", "message", "details"? } }`:

| Status | `code` | When |
| --- | --- | --- |
| 400 | `INVALID_REQUEST`, `INVALID_JSON`, `INVALID_MAX_CYCLES` | Bad request body. |
| 404 | `NOT_FOUND` | Unknown route or example. |
| 413 | `SOURCE_TOO_LARGE`, `OUTPUT_TOO_LARGE` | Program or result too large. |
| 422 | `ASSEMBLY_FAILED` | `details.diagnostics` lists every error as `{ line, column, message }`. |
| 503 | `SIMULATOR_UNAVAILABLE` | `rvsim`/`rvasm` not built. |
| 504 | `SIMULATION_TIMEOUT` | The run took longer than `RUN_TIMEOUT_MS`. |
| 500 | `SIMULATOR_ERROR`, `INTERNAL_ERROR` | Unexpected simulator output. |

Responses are gzip-compressed when the client accepts it; timelines shrink roughly tenfold.
