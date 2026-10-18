import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import vm from 'node:vm';

const dashboard = readFileSync(new URL('../src/components/RuntimeDashboard.vue', import.meta.url), 'utf8');
const start = dashboard.indexOf('async function onRuntimeCheckbox(');
const end = dashboard.indexOf('\nfunction handleCheckboxChange(', start);
assert.ok(start >= 0 && end > start, 'the production checkbox handler must be available');
const handlerSource = dashboard.slice(start, end);

function createHarness({ ok = true, failure } = {}) {
  const requests = [], notifications = [], timers = new Map();
  const state = { enabled: false };
  let nextTimer = 1, readbacks = 0;
  // [MOCKED!] HTTP transport and timers; execute the unchanged production handler.
  const context = vm.createContext({
    checkboxDebounceTimer: null,
    rURIComp: encodeURIComponent,
    fetch: async (url, options) => {
      requests.push({ url, options });
      if (failure) throw failure;
      return new Response(null, { status: ok ? 200 : 503 });
    },
    notifySafe: (...args) => notifications.push(args),
    fetchRuntime: () => { readbacks++; state.enabled = false; },
    setTimeout: (callback, delay) => {
      const id = nextTimer++;
      timers.set(id, { callback, delay });
      return id;
    },
    clearTimeout: (id) => timers.delete(id),
  });
  vm.runInContext(handlerSource, context);
  return {
    requests, notifications, state, timers,
    get readbacks() { return readbacks; },
    change: (...args) => context.onRuntimeCheckbox(...args),
    async flush() {
      const [id, timer] = timers.entries().next().value;
      timers.delete(id);
      assert.equal(timer.delay, 160);
      await timer.callback();
    },
  };
}

for (const requested of [true, false]) {
  test(`HTTP 200 acknowledges the request without claiming actual ${requested ? 'ON' : 'OFF'} state`, async () => {
    const harness = createHarness();
    await harness.change('Battery control', 'charge/mos', requested);
    assert.equal(harness.requests.length, 0, 'the request remains debounced');
    assert.equal(harness.state.enabled, false, 'no optimistic state update');
    await harness.flush();
    assert.equal(harness.requests.length, 1);
    assert.equal(harness.requests[0].url,
      `/runtime_action/checkbox?group=Battery%20control&key=charge%2Fmos&value=${requested}`);
    assert.equal(harness.requests[0].options.method, 'POST');
    assert.deepEqual(harness.notifications, [['Request sent: charge/mos', 'info', 1200]]);
    assert.equal(harness.readbacks, 1, 'the actual state is requested again');
    assert.equal(harness.state.enabled, false, 'a rejected async operation remains off after readback');
  });
}

test('HTTP failure reports an error without a success toast or readback', async () => {
  const harness = createHarness({ ok: false });
  await harness.change('Battery', 'chargeMos', true);
  await harness.flush();
  assert.deepEqual(harness.notifications, [['Toggle failed: Battery/chargeMos', 'error']]);
  assert.equal(harness.readbacks, 0);
  assert.equal(harness.state.enabled, false);
});

test('transport failure reports an error without claiming a state change', async () => {
  const harness = createHarness({ failure: new Error('[MOCKED!] connection lost') });
  await harness.change('Battery', 'chargeMos', true);
  await harness.flush();
  assert.deepEqual(harness.notifications, [['Toggle error chargeMos: [MOCKED!] connection lost', 'error']]);
  assert.equal(harness.readbacks, 0);
});

test('the existing debounce sends only the latest requested value', async () => {
  const harness = createHarness();
  await harness.change('Battery', 'chargeMos', true);
  await harness.change('Battery', 'chargeMos', false);
  assert.equal(harness.timers.size, 1);
  await harness.flush();
  assert.equal(harness.requests.length, 1);
  assert.match(harness.requests[0].url, /value=false$/);
  assert.equal(harness.readbacks, 1);
});
