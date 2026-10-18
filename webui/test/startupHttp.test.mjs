import assert from "node:assert/strict";
import test from "node:test";
import { readTextResponse } from "../src/startupHttp.mjs";

// [MOCKED!] Separate header/body barriers exercise browser fetch consumption.
test("startup waits for the complete body, preserves status and request options", async () => {
  let releaseBody, settled = false;
  const pendingBody = new Promise(resolve => { releaseBody = resolve; });
  const request = readTextResponse("/theme", { options: { headers: { probe: "yes" } },
    fetcher: async (url, options) => {
      assert.equal(url, "/theme");
      assert.equal(options.headers.probe, "yes");
      return { ok: false, status: 403, text: () => pendingBody };
    } }).then(value => { settled = true; return value; });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(settled, false);
  releaseBody("denied");
  assert.deepEqual(await request, { ok: false, status: 403, body: "denied" });
});

for (const body of [false, true]) {
  test(`bounded startup ${body ? "body" : "headers"} prevents an infinite wait`, async () => {
    let signal;
    await assert.rejects(readTextResponse("/slow", { timeoutMs: 10,
      fetcher: async (_, options) => { signal = options.signal;
        const pending = new Promise(() => {});
        return body ? { ok: true, status: 200, text: () => pending } : pending;
      } }), { name: "AbortError" });
    assert.equal(signal.aborted, true);
  });
}

test("disposed startup aborts the body and rejects before later stages", async () => {
  const controller = new AbortController();
  let requestSignal;
  const pending = readTextResponse("/theme", { signal: controller.signal,
    fetcher: async (_, options) => { requestSignal = options.signal;
      return { ok: true, status: 200, text: () => new Promise(() => {}) };
    } });
  controller.abort();
  await assert.rejects(pending, { name: "AbortError" });
  assert.equal(requestSignal.aborted, true);
});
