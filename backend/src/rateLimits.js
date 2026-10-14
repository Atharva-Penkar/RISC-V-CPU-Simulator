import { rateLimit } from "express-rate-limit";

/**
 * Per-IP request limits, counted in this server's memory. Cloud Run runs at most a couple of instances,
 * so a client can get at most that multiple of a limit, which is acceptable for keeping one visitor or
 * script from monopolising the simulator. They add to the hard limits on every run (source size,
 * timeout, concurrent runs).
 *
 * Over a limit, the response is 429 with the API's usual error body, plus Retry-After and the standard
 * RateLimit headers.
 */

const MINUTE = 60 * 1000;
const HOUR = 60 * MINUTE;

/**
 * @param {{ windowMs: number, limit: number, what: string }} options
 */
function limiter({ windowMs, limit, what }) {
  return rateLimit({
    windowMs,
    limit,
    standardHeaders: "draft-8",
    legacyHeaders: false,
    handler(req, res, next, options) {
      const period = windowMs === HOUR ? "hour" : "minute";
      res.status(options.statusCode).json({
        error: {
          code: "RATE_LIMITED",
          message: `Too many ${what}: the limit is ${limit} per ${period}. Wait a little and try again.`,
        },
      });
    },
  });
}

/**
 * The limiters for the API, from config.rateLimits; empty when rate limiting is off.
 * @param {{ enabled: boolean, runsPerMinute: number, runsPerHour: number, readsPerMinute: number }} limits
 */
export function createRateLimits(limits) {
  if (!limits.enabled) return { runs: [], reads: [] };
  return {
    // POST /api/simulate and /api/assemble start a simulator process: the expensive requests.
    runs: [
      limiter({ windowMs: MINUTE, limit: limits.runsPerMinute, what: "runs" }),
      limiter({ windowMs: HOUR, limit: limits.runsPerHour, what: "runs" }),
    ],
    // The GET endpoints are cheap; this only stops floods.
    reads: [limiter({ windowMs: MINUTE, limit: limits.readsPerMinute, what: "requests" })],
  };
}
