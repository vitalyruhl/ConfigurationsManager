function abortError() { return new DOMException('Request timeout or cancellation', 'AbortError'); }

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

export function createJsonCache(request) {
  let promise;
  return { load() {
    if (!promise) promise = Promise.resolve().then(request).catch(error => { promise = undefined; throw error; });
    return promise;
  } };
}

const metadata = createJsonCache(async () => {
  const value = await readJson('/runtime_meta.json');
  if (!Array.isArray(value)) throw new Error('Runtime metadata response was invalid.');
  return value;
});
const layout = createJsonCache(async () => {
  const value = await readJson('/live_layout.json');
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Live layout response was invalid.');
  return value;
});
const discovery = createJsonCache(() => readJson('/runtime.json'));
export const loadRuntimeMeta = () => metadata.load();
export const loadLiveLayout = () => layout.load();
// Only metadata-less legacy dashboards need one shared full discovery request.
export const loadRuntimeDiscovery = () => discovery.load();


const pageIndex = createJsonCache(async () => {
  const value = await readJson('/live_pages.json');
  if (!value || typeof value.pageScoped !== 'boolean' || typeof value.hasLiveContent !== 'boolean' ||
      !Array.isArray(value.pages) || typeof value.defaultPage !== 'string' ||
      value.pages.some(page => !page || typeof page.key !== 'string' || !page.key.length || new TextEncoder().encode(page.key).length > 256) ||
      new Set(value.pages.map(page => page.key)).size !== value.pages.length) {
    throw new Error('Live page index response was invalid.');
  }
  return value;
});
export const loadLivePages = () => pageIndex.load();

// One serialized request chain avoids overlapping page-generation peaks on ESP32.
// Successful pages live only in browser memory; failures and cancellations retry.
export function createPageDataClient(request = readJson) {
  const cache = new Map();
  let tail = Promise.resolve();
  return { load(key, signal) {
    const cached = cache.get(key);
    if (cached && (!cached.pending || !cached.signal?.aborted)) return cached.promise;
    const entry = { signal, pending: true, promise: null };
    const checkAbort = () => { if (signal?.aborted) throw abortError(); };
    entry.promise = tail.catch(() => {}).then(async () => {
      checkAbort();
      const query = `?page=${encodeURIComponent(key)}`;
      const layout = await request(`/live_layout.json${query}`, { signal });
      checkAbort();
      if (!layout || !Array.isArray(layout.pages) ||
          (key === '__unassigned__' ? layout.pages.length !== 0 : layout.pages.length !== 1 || layout.pages[0]?.key !== key)) {
        throw new Error('Selected page layout response was invalid.');
      }
      const metadata = await request(`/runtime_meta.json${query}`, { signal });
      checkAbort();
      if (!Array.isArray(metadata)) throw new Error('Selected page metadata response was invalid.');
      entry.pending = false;
      return { layout, metadata };
    }).catch(error => {
      if (cache.get(key) === entry) cache.delete(key);
      throw error;
    });
    cache.set(key, entry);
    tail = entry.promise.catch(() => {});
    return entry.promise;
  } };
}
export const livePageData = createPageDataClient();
