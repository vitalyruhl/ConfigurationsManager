import assert from "node:assert/strict";
import test from "node:test";
import { createAppInfoRecovery } from "../src/appInfoClient.mjs";

// [MOCKED!] HTTP responses and retry timers replace the ESP32/browser transport.
const json = (value) => ({ ok: true, json: async () => value });
const text = (value) => ({ ok: true, text: async () => value });
const identity = { appName: "SolarBatteryTelemetry", appTitle: "Battery", version: "1.0.0", guiLogging: true };
const flush = () => new Promise(resolve => setImmediate(resolve));
function timers() {
  const pending = new Map();
  let next = 0;
  return {
    setTimer(fn, delay) { pending.set(++next, { fn, delay }); return next; },
    clearTimer(id) { pending.delete(id); },
    get size() { return pending.size; },
    async next() {
      const [id, { fn, delay }] = pending.entries().next().value;
      pending.delete(id); fn(); await flush(); return delay;
    },
  };
}
function setup(fetcher, options = {}) {
  const updates = [], clock = timers();
  const controller = createAppInfoRecovery({ fetcher, onUpdate: info => updates.push(info), ...clock, ...options });
  return { controller, updates, clock };
}

test("valid appinfo updates identity and explicitly confirmed GUI logging", async () => {
  const calls = [];
  const { controller, updates, clock } = setup(async url => { calls.push(url); return json(identity); });
  await controller.run();
  assert.deepEqual(calls, ["/appinfo"]);
  assert.deepEqual(updates, [identity]);
  assert.equal(clock.size, 0);
  controller.stop();
});

for (const [label, failure] of [
  ["transport failure", () => { throw new TypeError("Connection reset"); }],
  ["HTTP failure", () => ({ ok: false, status: 503 })],
  ["invalid JSON", () => ({ ok: true, json: async () => { throw new SyntaxError("Truncated"); } })],
  ["invalid shape", () => json({ guiLogging: true })],
]) {
  test(`${label} still tries legacy version and schedules recovery`, async () => {
    const calls = [];
    const { controller, updates, clock } = setup(async url => {
      calls.push(url); return url === "/appinfo" ? failure() : text("SolarBatteryTelemetry 1.0.0");
    });
    await controller.run();
    assert.deepEqual(calls, ["/appinfo", "/version"]);
    assert.deepEqual(updates, [{ appName: "SolarBatteryTelemetry", appTitle: "", version: "1.0.0", guiLogging: false }]);
    assert.equal(clock.size, 1);
    controller.stop();
    assert.equal(clock.size, 0);
  });
}

test("transient total failure retains identity but disables GUI logging", async () => {
  let online = true;
  const { controller, updates } = setup(async () => {
    if (!online) throw new TypeError("Offline");
    return json(identity);
  });
  await controller.run(); online = false; await controller.run();
  assert.deepEqual(updates.at(-1), { ...identity, guiLogging: false });
  controller.stop();
});

test("legacy fallback preserves previously known title for the same app", async () => {
  let online = true;
  const { controller, updates } = setup(async url => {
    if (online) return json(identity);
    if (url === "/appinfo") throw new TypeError("Offline");
    return text("SolarBatteryTelemetry 1.0.0");
  });
  await controller.run(); online = false; await controller.run();
  assert.deepEqual(updates.at(-1), { ...identity, guiLogging: false });
  controller.stop();
});

test("bounded retries recover after reboot and repeated run calls do not overlap", async () => {
  let calls = 0, online = false;
  const { controller, updates, clock } = setup(async () => {
    calls++; if (!online) throw new TypeError("Rebooting"); return json(identity);
  });
  const first = controller.run();
  assert.equal(controller.run(), first);
  await first;
  await controller.run();
  assert.equal(calls, 2);
  assert.equal(await clock.next(), 2000);
  assert.equal(calls, 4);
  online = true;
  assert.equal(await clock.next(), 5000);
  assert.equal(calls, 5);
  assert.deepEqual(updates.at(-1), identity);
  assert.equal(clock.size, 0);
  controller.stop();
});

test("persistent failures stop after three retries", async () => {
  let calls = 0;
  const { controller, clock } = setup(async () => { calls++; throw new Error("Offline"); });
  await controller.run();
  assert.equal(await clock.next(), 2000);
  assert.equal(await clock.next(), 5000);
  assert.equal(await clock.next(), 10000);
  assert.equal(clock.size, 0);
  assert.equal(calls, 8);
  controller.stop();
});

for (const body of [false, true]) {
  test(`timeout covers stalled ${body ? "response body" : "fetch"} and allows fallback`, async () => {
    let primarySignal;
    const { controller, updates } = setup(async (url, { signal }) => {
      if (url === "/version") return text("Recovered 2.0.0");
      primarySignal = signal;
      const pending = new Promise(() => {});
      return body ? { ok: true, json: () => pending } : pending;
    }, { timeoutMs: 10 });
    await controller.run();
    assert.equal(primarySignal.aborted, true);
    assert.equal(updates.at(-1).appName, "Recovered");
    controller.stop();
  });
}

test("unmount aborts active fetch and prevents fallback, updates and retries", async () => {
  const calls = [];
  let requestSignal;
  const { controller, updates, clock } = setup(async (url, { signal }) => {
    calls.push(url); requestSignal = signal; return new Promise(() => {});
  });
  const pending = controller.run();
  controller.stop(); await pending;
  assert.equal(requestSignal.aborted, true);
  assert.deepEqual(calls, ["/appinfo"]);
  assert.equal(updates.length, 0);
  assert.equal(clock.size, 0);
  await controller.run();
  assert.equal(calls.length, 1);
});

test("truthy non-boolean guiLogging does not enable GUI logging", async () => {
  const { controller, updates } = setup(async () => json({ ...identity, guiLogging: "false" }));
  await controller.run();
  assert.equal(updates.at(-1).guiLogging, false);
  controller.stop();
});


test("startup can pause recovery retries until schema bootstrap completes", async () => {
  let calls = 0;
  const { controller, clock } = setup(async () => { calls++; throw new Error("Offline"); });
  controller.pauseRetries();
  await controller.run();
  assert.equal(calls, 2);
  assert.equal(clock.size, 0);
  await controller.run();
  assert.equal(calls, 2);
  controller.resumeRetries();
  assert.equal(clock.size, 1);
  assert.equal(await clock.next(), 2000);
  assert.equal(calls, 4);
  controller.stop();
});

test("pausing an already queued retry preserves it and stop prevents resume", async () => {
  const { controller, clock } = setup(async () => { throw new Error("Offline"); });
  await controller.run();
  controller.pauseRetries();
  assert.equal(clock.size, 0);
  controller.resumeRetries();
  assert.equal(await clock.next(), 2000);
  controller.pauseRetries();
  controller.stop();
  controller.resumeRetries();
  assert.equal(clock.size, 0);
});
