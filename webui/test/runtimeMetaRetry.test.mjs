import assert from "node:assert/strict";
import test from "node:test";

import { createRuntimeMetaRetryController } from "../src/runtimeMetaRetry.mjs";

function createFakeTimers() {
  let nextId = 1;
  const callbacks = new Map();
  return {
    setTimer(callback) {
      const id = nextId++;
      callbacks.set(id, callback);
      return id;
    },
    clearTimer(id) {
      callbacks.delete(id);
    },
    get count() {
      return callbacks.size;
    },
    async runOne() {
      const entry = callbacks.entries().next().value;
      assert.ok(entry, "expected one retry timer");
      const [id, callback] = entry;
      callbacks.delete(id);
      callback();
      await new Promise((resolve) => setImmediate(resolve));
    },
  };
}

test("retries a 503 once and preserves metadata until a successful response", async () => {
  const timers = createFakeTimers();
  const existingMeta = [{ key: "existing" }];
  const refreshedMeta = [{ key: "updated" }];
  let visibleMeta = existingMeta;
  let visibleMessage = "";
  let requestCount = 0;
  const results = [
    { ok: false, retryable: true, message: "Runtime data is temporarily unavailable." },
    { ok: true, data: refreshedMeta },
  ];

  const controller = createRuntimeMetaRetryController({
    request: async () => {
      requestCount += 1;
      return results.shift();
    },
    onSuccess: (meta) => {
      visibleMeta = meta;
      visibleMessage = "";
    },
    onFailure: (message, retryable) => {
      visibleMessage = retryable ? `${message} Retrying in 5 seconds…` : message;
    },
    retryDelayMs: 5000,
    setTimer: timers.setTimer,
    clearTimer: timers.clearTimer,
  });

  await controller.run();
  assert.equal(requestCount, 1);
  assert.equal(visibleMeta, existingMeta);
  assert.match(visibleMessage, /Retrying in 5 seconds/);
  assert.equal(timers.count, 1);
  assert.deepEqual(controller.state(), {
    requestInFlight: false,
    retryScheduled: true,
    permanentFailure: false,
  });

  await timers.runOne();
  assert.equal(requestCount, 2);
  assert.equal(visibleMeta, refreshedMeta);
  assert.equal(visibleMessage, "");
  assert.equal(timers.count, 0);
  assert.deepEqual(controller.state(), {
    requestInFlight: false,
    retryScheduled: false,
    permanentFailure: false,
  });
});

test("repeated temporary failures keep one request and one retry timer", async () => {
  const timers = createFakeTimers();
  let requestCount = 0;
  let resolveRequest;
  const controller = createRuntimeMetaRetryController({
    request: () => {
      requestCount += 1;
      return new Promise((resolve) => {
        resolveRequest = resolve;
      });
    },
    onSuccess: () => {},
    onFailure: () => {},
    retryDelayMs: 5000,
    setTimer: timers.setTimer,
    clearTimer: timers.clearTimer,
  });

  const first = controller.run();
  await controller.run();
  assert.equal(requestCount, 1);
  assert.equal(timers.count, 0);

  resolveRequest({ ok: false, retryable: true, message: "Runtime data is temporarily unavailable." });
  await first;
  assert.equal(timers.count, 1);
  await controller.run();
  assert.equal(requestCount, 1);
  assert.equal(timers.count, 1);

  await timers.runOne();
  assert.equal(requestCount, 2);
  resolveRequest({ ok: false, retryable: true, message: "Runtime data is temporarily unavailable." });
  await new Promise((resolve) => setImmediate(resolve));
  assert.equal(timers.count, 1);
});
