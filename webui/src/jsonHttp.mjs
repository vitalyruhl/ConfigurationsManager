export function abortError() { return new DOMException('Request timeout or cancellation', 'AbortError'); }

// Keep cancellation active until the response body is fully consumed.
export async function readJson(resource, options = {}, timeoutMs = 5000, fetcher = fetch) {
  const controller = new AbortController();
  let timeout, rejectAbort;
  const aborted = new Promise((_, reject) => { rejectAbort = reject; });
  const abort = () => { controller.abort(); rejectAbort(abortError()); };
  options.signal?.addEventListener('abort', abort, { once: true });
  timeout = setTimeout(abort, timeoutMs);
  if (options.signal?.aborted) abort();
  try {
    return await Promise.race([aborted, (async () => {
      const response = await fetcher(resource, { ...options, signal: controller.signal });
      if (!response.ok) {
        const error = new Error(`Request failed (HTTP ${response.status}).`);
        error.status = response.status;
        throw error;
      }
      return await response.json();
    })()]);
  } finally {
    clearTimeout(timeout);
    options.signal?.removeEventListener('abort', abort);
  }
}
