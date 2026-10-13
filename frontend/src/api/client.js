// fetch wrapper for the simulator API (backend/README.md). Every failure becomes an ApiError with the
// backend's { error: { code, message, details } } fields; a cancelled request rejects with the
// AbortError from fetch, which callers ignore (see isAbortError).

/** Base URL without a trailing slash; empty means the same origin (the dev server proxies /api). */
export const API_BASE_URL = (import.meta.env?.VITE_API_BASE_URL ?? "").replace(/\/+$/, "");

export class ApiError extends Error {
  /**
   * @param {number} status   HTTP status, or 0 when the server could not be reached
   * @param {string} code     e.g. "ASSEMBLY_FAILED", "SIMULATION_TIMEOUT", "NETWORK_ERROR"
   * @param {string} message
   * @param {any} [details]   e.g. { diagnostics: [...] } for ASSEMBLY_FAILED
   */
  constructor(status, code, message, details) {
    super(message);
    this.name = "ApiError";
    this.status = status;
    this.code = code;
    this.details = details;
  }
}

/**
 * True for the rejection of a request cancelled with its AbortController.
 * @param {unknown} error
 */
export function isAbortError(error) {
  return error instanceof Error && error.name === "AbortError";
}

/**
 * @typedef {object} RequestOptions
 * @property {"GET" | "POST"} [method]
 * @property {unknown} [body]          sent as JSON
 * @property {AbortSignal} [signal]
 */

/**
 * Sends a request and returns the parsed JSON body of a successful response.
 * @param {string} path  e.g. "/api/simulate"
 * @param {RequestOptions} [options]
 * @returns {Promise<any>}
 */
export async function request(path, { method = "GET", body, signal } = {}) {
  let response;
  try {
    response = await fetch(API_BASE_URL + path, {
      method,
      headers: body === undefined ? undefined : { "Content-Type": "application/json" },
      body: body === undefined ? undefined : JSON.stringify(body),
      signal,
    });
  } catch (error) {
    if (isAbortError(error)) throw error;
    throw new ApiError(0, "NETWORK_ERROR", "Could not reach the simulator API. Check your connection and try again.");
  }

  let data = null;
  try {
    data = await response.json();
  } catch (error) {
    if (isAbortError(error)) throw error;
  }

  if (!response.ok) {
    const error = data?.error;
    throw new ApiError(
      response.status,
      typeof error?.code === "string" ? error.code : "HTTP_ERROR",
      typeof error?.message === "string" ? error.message : `The API answered with status ${response.status}.`,
      error?.details,
    );
  }
  if (data === null) throw new ApiError(response.status, "INVALID_RESPONSE", "The API returned a response that is not JSON.");
  return data;
}
