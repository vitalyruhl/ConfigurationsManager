// Metadata must define tabs before their providers have been selected/evaluated.
export function runtimeLayoutGroups(groups, hasMetadata, hasVisibleValue) {
  return groups.filter(group => hasMetadata
    ? Array.isArray(group.fields) && group.fields.some(field => field && !field.isDivider)
    : hasVisibleValue(group));
}

export function runtimeSources(cards, active) {
  if (!active) return [];
  const groups = [...new Set((cards || []).filter(card => card.visible !== false && card.enabled !== false)
    .flatMap(card => card.runtimeSources || []))].sort();
  const bytes = text => new TextEncoder().encode(text).length;
  if (groups.length > 32 || groups.some(group => typeof group !== 'string' || !group.length || bytes(group) > 64 || /[\x00-\x1f]/.test(group)) || bytes(JSON.stringify(groups)) > 2048) {
    throw new Error('The visible dashboard exceeds the runtime group selection limit.');
  }
  return groups;
}

export function createLatestRuntimeRequest({ request, onData, onError = () => {} }) {
  let groups = null, key = null, generation = 0, inFlight = null, controller = null, queued = false, disposed = false;
  function run() {
    if (disposed || groups === null) return Promise.resolve();
    if (inFlight) { queued = true; return inFlight; }
    const current = generation;
    controller = new AbortController();
    const requested = [...groups], signal = controller.signal;
    inFlight = (async () => request(requested, signal))()
      .then(data => { if (!disposed && current === generation) onData(data); })
      .catch(error => { if (!disposed && current === generation && error.name !== 'AbortError') onError(error); })
      .finally(() => { inFlight = null; if (queued && !disposed) { queued = false; void run(); } });
    return inFlight;
  }
  function select(next) {
    const nextKey = JSON.stringify(next);
    if (nextKey === key) return run();
    key = nextKey; groups = [...next]; generation++;
    controller?.abort();
    return run();
  }
  function suspend() {
    generation++;
    queued = false;
    groups = null;
    key = null;
    controller?.abort();
  }
  function cancel() { disposed = true; suspend(); }
  return { run, select, suspend, cancel };
}
