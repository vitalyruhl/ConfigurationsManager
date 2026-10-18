// Bound both header and body reads: an ESP reboot can interrupt either phase.
async function readResponse(url, kind, signal, timeoutMs, fetcher) {
  const controller = new AbortController();
  let rejectAbort;
  const aborted = new Promise((_, reject) => { rejectAbort = reject; });
  const abort = () => {
    controller.abort();
    rejectAbort(new DOMException("App identity request cancelled or timed out", "AbortError"));
  };
  signal.addEventListener("abort", abort, { once: true });
  const timer = setTimeout(abort, timeoutMs);
  if (signal.aborted) abort();
  try {
    return await Promise.race([aborted, (async () => {
      const response = await fetcher(url, { signal: controller.signal, cache: "no-store" });
      if (!response.ok) throw new Error(`App identity request failed (HTTP ${response.status}).`);
      return await response[kind]();
    })()]);
  } finally {
    clearTimeout(timer);
    signal.removeEventListener("abort", abort);
  }
}

const clean = value => typeof value === "string" ? value.trim() : "";

export function createAppInfoRecovery({
  onUpdate,
  fetcher = fetch,
  timeoutMs = 4000,
  retryDelays = [2000, 5000, 10000],
  setTimer = setTimeout,
  clearTimer = clearTimeout,
}) {
  let stopped = false, inFlight = null, retryTimer = null, retryIndex = 0;
  let activeController = null, latest = null;
  let retriesPaused = false, pendingRetryDelay = null;

  function scheduleRetry() {
    if (stopped || retriesPaused || retryTimer !== null || pendingRetryDelay === null) return;
    retryTimer = setTimer(() => {
      retryTimer = null;
      pendingRetryDelay = null;
      attempt();
    }, pendingRetryDelay);
  }

  function publish(info) {
    if (stopped) return;
    latest = info;
    onUpdate(info);
  }

  function attempt() {
    const controller = new AbortController();
    activeController = controller;
    inFlight = (async () => {
      let structured = false;
      try {
        const info = await readResponse("/appinfo", "json", controller.signal, timeoutMs, fetcher);
        if (!info || typeof info !== "object" || Array.isArray(info) || !clean(info.appName)) {
          throw new Error("Invalid app identity response.");
        }
        if (stopped) return;
        publish({ appName: clean(info.appName), appTitle: clean(info.appTitle),
          version: clean(info.version), guiLogging: info.guiLogging === true });
        structured = true;
      } catch {
        if (stopped) return;
        // Keep the last identity while a reboot or a broken response is transient.
        if (latest) publish({ ...latest, guiLogging: false });
        try {
          const raw = clean(await readResponse("/version", "text", controller.signal, timeoutMs, fetcher));
          if (!raw) throw new Error("Empty legacy app identity.");
          const match = raw.match(/^(.*)\s+(\d+\.\d+\.\d+(?:[-+].*)?)$/);
          const appName = match ? match[1].trim() : raw;
          publish({ appName, appTitle: latest?.appName === appName ? latest.appTitle : "",
            version: match ? match[2] : "", guiLogging: false });
        } catch {
          // Preserve the last successful identity; the bounded retry handles recovery.
        }
      } finally {
        inFlight = null;
        activeController = null;
        if (!stopped && !structured && retryIndex < retryDelays.length) {
          pendingRetryDelay = retryDelays[retryIndex++];
          scheduleRetry();
        }
      }
    })();
    return inFlight;
  }

  return {
    run() {
      if (stopped) return Promise.resolve();
      if (inFlight) return inFlight;
      if (retryTimer !== null || pendingRetryDelay !== null) return Promise.resolve();
      retryIndex = 0;
      return attempt();
    },
    pauseRetries() {
      retriesPaused = true;
      if (retryTimer !== null) clearTimer(retryTimer);
      retryTimer = null;
    },
    resumeRetries() {
      retriesPaused = false;
      scheduleRetry();
    },
    stop() {
      stopped = true;
      pendingRetryDelay = null;
      if (retryTimer !== null) clearTimer(retryTimer);
      retryTimer = null;
      activeController?.abort();
    },
  };
}
