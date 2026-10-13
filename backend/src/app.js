import compression from "compression";
import express from "express";
import { ApiError } from "./simulator.js";

/** Reads and checks { source } from a request body. */
function readSource(body, config) {
  if (typeof body?.source !== "string") {
    throw new ApiError(400, "INVALID_REQUEST", 'The request body must be JSON with a string field "source".');
  }
  if (Buffer.byteLength(body.source, "utf8") > config.maxSourceBytes) {
    throw new ApiError(413, "SOURCE_TOO_LARGE", `The program is larger than ${config.maxSourceBytes} bytes.`);
  }
  return body.source;
}

/** Reads the optional maxCycles field: an integer from 1 to the configured limit. */
function readMaxCycles(body, config) {
  const value = body.maxCycles ?? config.defaultMaxCycles;
  if (!Number.isInteger(value) || value < 1 || value > config.maxCyclesLimit) {
    throw new ApiError(400, "INVALID_MAX_CYCLES", `"maxCycles" must be an integer from 1 to ${config.maxCyclesLimit}.`);
  }
  return value;
}

/** The rvsim option for each latency, and the largest value it accepts. */
const LATENCY_FLAGS = { multiply: "--mul-latency", divide: "--div-latency", load: "--load-latency", store: "--store-latency" };
export const MAX_LATENCY = 100;

/**
 * Optional cycles for multiply, divide, load and store: { multiply?, divide?, load?, store? }, each 1 to 100.
 * Returns the rvsim arguments for the ones given.
 */
function readLatencies(body) {
  const latencies = body.latencies ?? {};
  if (typeof latencies !== "object" || latencies === null || Array.isArray(latencies)) {
    throw new ApiError(400, "INVALID_LATENCIES", '"latencies" must be an object.');
  }
  const args = [];
  for (const [name, value] of Object.entries(latencies)) {
    const flag = LATENCY_FLAGS[name];
    if (!flag) {
      throw new ApiError(400, "INVALID_LATENCIES", `Unknown latency "${name}". Use multiply, divide, load or store.`);
    }
    if (!Number.isInteger(value) || value < 1 || value > MAX_LATENCY) {
      throw new ApiError(400, "INVALID_LATENCIES", `Latency "${name}" must be an integer from 1 to ${MAX_LATENCY}.`);
    }
    args.push(flag, String(value));
  }
  return args;
}

/** Optional "forwarding": false runs the pipeline without forwarding paths. */
function readForwarding(body) {
  const forwarding = body.forwarding ?? true;
  if (typeof forwarding !== "boolean") {
    throw new ApiError(400, "INVALID_FORWARDING", '"forwarding" must be true or false.');
  }
  return forwarding ? [] : ["--no-forwarding"];
}

/**
 * The HTTP API:
 *   GET  /api/health          simulator availability and limits
 *   GET  /api/examples        example programs
 *   GET  /api/examples/:id    one example program
 *   POST /api/assemble        { source } -> "rv-program" document
 *   POST /api/simulate        { source, maxCycles?, latencies?, forwarding? } -> "rv-timeline" document
 * Errors are { error: { code, message, details? } }; assembly errors are 422 with the diagnostics as details.
 */
export function createApp({ config, simulator, examples }) {
  const app = express();
  app.disable("x-powered-by");
  app.use(compression());   // timelines are repetitive JSON and shrink about tenfold

  app.use((req, res, next) => {
    res.set("Access-Control-Allow-Origin", config.corsOrigin);
    res.set("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set("Access-Control-Allow-Headers", "Content-Type");
    if (req.method === "OPTIONS") return res.status(204).end();
    next();
  });
  // Room for JSON escaping on top of the source size limit; readSource enforces the real limit.
  app.use(express.json({ limit: config.maxSourceBytes * 2 + 1024 }));

  app.get("/api/health", async (req, res) => {
    const available = await simulator.available();
    res.json({
      status: available.rvsim && available.rvasm ? "ok" : "degraded",
      simulator: available,
      limits: {
        maxSourceBytes: config.maxSourceBytes,
        defaultMaxCycles: config.defaultMaxCycles,
        maxCyclesLimit: config.maxCyclesLimit,
        maxLatency: MAX_LATENCY,
      },
    });
  });

  app.get("/api/examples", (req, res) => {
    res.json({ examples: examples.map(({ id, description }) => ({ id, description })) });
  });

  app.get("/api/examples/:id", (req, res) => {
    const example = examples.find((e) => e.id === req.params.id);
    if (!example) throw new ApiError(404, "NOT_FOUND", `No example named "${req.params.id}".`);
    res.json(example);
  });

  app.post("/api/assemble", async (req, res) => {
    const source = readSource(req.body, config);
    res.json(await simulator.assemble(source));
  });

  app.post("/api/simulate", async (req, res) => {
    const source = readSource(req.body, config);
    const maxCycles = readMaxCycles(req.body, config);
    const pipelineArgs = [...readLatencies(req.body), ...readForwarding(req.body)];
    res.json(await simulator.simulate(source, maxCycles, pipelineArgs));
  });

  app.use((req, res) => {
    res.status(404).json({ error: { code: "NOT_FOUND", message: `No route for ${req.method} ${req.path}.` } });
  });

  // eslint-disable-next-line no-unused-vars -- Express recognises error handlers by their four parameters
  app.use((error, req, res, next) => {
    if (error.type === "entity.parse.failed") {
      error = new ApiError(400, "INVALID_JSON", "The request body is not valid JSON.");
    } else if (error.type === "entity.too.large") {
      error = new ApiError(413, "SOURCE_TOO_LARGE", "The request body is too large.");
    }
    if (!(error instanceof ApiError)) {
      console.error(error);
      error = new ApiError(500, "INTERNAL_ERROR", "Something went wrong while running the simulator.");
    }
    const body = { error: { code: error.code, message: error.message } };
    if (error.details !== undefined) body.error.details = error.details;
    res.status(error.status).json(body);
  });

  return app;
}
