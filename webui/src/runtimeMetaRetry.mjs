export const RUNTIME_META_RETRY_MS = 5000;

export function createRuntimeMetaRetryController({
  request,
  onSuccess,
  onFailure,
  retryDelayMs = RUNTIME_META_RETRY_MS,
  setTimer = setTimeout,
  clearTimer = clearTimeout,
}) {
  let requestInFlight = false;
  let retryTimer = null;
  let permanentFailure = false;
  let disposed = false;

  function scheduleRetry() {
    if (disposed || retryTimer !== null) return false;
    retryTimer = setTimer(() => {
      retryTimer = null;
      void run();
    }, retryDelayMs);
    return true;
  }

  async function run() {
    if (disposed || requestInFlight || retryTimer !== null || permanentFailure) {
      return false;
    }

    requestInFlight = true;
    try {
      const result = await request();
      if (result?.ok) {
        onSuccess(result.data);
        return true;
      }

      const retryable = result?.retryable === true;
      onFailure(result?.message || "Runtime data is unavailable.", retryable);
      if (retryable) {
        scheduleRetry();
      } else {
        permanentFailure = true;
      }
      return false;
    } catch {
      onFailure("Runtime data is temporarily unavailable.", true);
      scheduleRetry();
      return false;
    } finally {
      requestInFlight = false;
    }
  }

  function cancel() {
    disposed = true;
    if (retryTimer !== null) {
      clearTimer(retryTimer);
      retryTimer = null;
    }
  }

  function state() {
    return {
      requestInFlight,
      retryScheduled: retryTimer !== null,
      permanentFailure,
    };
  }

  return { run, cancel, state };
}
