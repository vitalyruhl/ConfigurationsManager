// Bound the entire header/body read for the optional theme and OTA status probe.
// Completing a browser body reduces overlap; server ACK cleanup can still follow.
export async function readTextResponse(resource, {
  signal, timeoutMs = 4000, options = {}, fetcher = fetch,
} = {}) {
  const controller = new AbortController();
  let rejectAbort;
  const aborted = new Promise((_, reject) => { rejectAbort = reject; });
  const abort = () => {
    controller.abort();
    rejectAbort(new DOMException("Startup request cancelled or timed out", "AbortError"));
  };
  signal?.addEventListener("abort", abort, { once: true });
  const timer = setTimeout(abort, timeoutMs);
  if (signal?.aborted) abort();
  try {
    return await Promise.race([aborted, (async () => {
      const response = await fetcher(resource, { ...options, signal: controller.signal });
      return { ok: response.ok, status: response.status, body: await response.text() };
    })()]);
  } finally {
    clearTimeout(timer);
    signal?.removeEventListener("abort", abort);
  }
}
