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
{ "source": "addi x1, x0, 5\nadd x2, x1, x1\nebreak", "maxCycles": 500, "latencies": { "multiply": 3, "load": 2 } }
```

`maxCycles` is optional. `latencies` is optional too: the cycles a multiply or divide spends in EX and a load or
store in MEM (`multiply`, `divide`, `load`, `store`, each 1 to 100, default 1). `"forwarding": false` runs the
pipeline without forwarding, so dependent instructions wait for write back. The answer (200) is the timeline document described in
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

## Deploying to Google Cloud Run

The `Dockerfile` in the repository root builds the image: it compiles `rvsim` and `rvasm` with CMake, then runs this
server on Node.js 22 with the example programs. Cloud Run sets `PORT`; the server listens on it.

1. **Project and billing.** In the [Google Cloud console](https://console.cloud.google.com/), create a project and
   link a billing account. Cloud Run needs one even within its free tier. Under *Billing → Budgets & alerts*, add a
   small budget (for example $1) so you are emailed before anything costs money.
2. **gcloud CLI.** Install the [Google Cloud CLI](https://cloud.google.com/sdk/docs/install), then:
   ```sh
   gcloud auth login
   gcloud config set project YOUR_PROJECT_ID
   gcloud config set run/region asia-south1
   gcloud services enable run.googleapis.com cloudbuild.googleapis.com artifactregistry.googleapis.com
   ```
3. **Deploy** from the repository root (not from `backend/`). Cloud Build builds the `Dockerfile`, stores the image in
   Artifact Registry and starts the service; the first run asks to create a repository, answer yes.
   ```sh
   gcloud run deploy riscv-sim-api \
     --source . \
     --allow-unauthenticated \
     --min-instances 0 --max-instances 2 \
     --cpu 1 --memory 512Mi \
     --concurrency 8 \
     --timeout 30 \
     --set-env-vars CORS_ORIGIN=https://atharva-penkar.github.io
   ```
   - `--min-instances 0` lets the service scale to zero when idle, which keeps it within the free tier; the first
     request after a quiet period then waits a few seconds while an instance starts.
   - `--max-instances 2` caps how much it can scale (and cost) under heavy traffic.
   - `CORS_ORIGIN` is the website's origin (scheme and host, no path). Use `*` to allow any origin.
4. **Check it.** The command prints the service URL:
   ```sh
   curl https://riscv-sim-api-XXXXXXXX.asia-south1.run.app/api/health
   ```
5. **Point the frontend at it** with `VITE_API_BASE_URL=https://riscv-sim-api-XXXXXXXX.asia-south1.run.app` for
   production builds (see `frontend/.env.example`).

To update the service, run the same `gcloud run deploy` command again. Logs are under *Cloud Run → riscv-sim-api →
Logs* in the console, or `gcloud run services logs read riscv-sim-api`.

Each deploy stores a new image in Artifact Registry, whose free storage is small; delete old images now and then, or
add a cleanup policy to the `cloud-run-source-deploy` repository. Check the current free-tier limits on the
[Cloud Run pricing page](https://cloud.google.com/run/pricing).
