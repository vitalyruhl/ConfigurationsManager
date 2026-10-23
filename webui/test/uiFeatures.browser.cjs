// [MOCKED!] HTTP device, OTA responses, and WebSockets; actual production bundles render in Chromium.
// Usage: PLAYWRIGHT_MODULE=<existing module> PLAYWRIGHT_BROWSER_PATH=<existing browser>
// node webui/test/uiFeatures.browser.cjs <matrix output directory>
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const output = path.resolve(process.argv[2] || '.local/ui-feature-matrix');
let directory;
const server = http.createServer((request, response) => {
  const name = new URL(request.url, 'http://localhost').pathname;
  const inline = path.join(directory, '..', 'inlined.html');
  const file = name === '/' && fs.existsSync(inline) ? inline : path.join(directory, name === '/' ? 'index.html' : name);
  if (!fs.existsSync(file) || !fs.statSync(file).isFile()) { response.writeHead(404); response.end(); return; }
  response.setHeader('Content-Type', file.endsWith('.js') ? 'text/javascript' : file.endsWith('.css') ? 'text/css' : 'text/html');
  response.end(fs.readFileSync(file));
});

(async () => {
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const base = `http://127.0.0.1:${server.address().port}`;
  const browser = await chromium.launch({ headless: true, executablePath: process.env.PLAYWRIGHT_BROWSER_PATH || undefined });
  const results = {};
  try {
    for (const mode of (process.argv[3] || '11,01,10,00').split(',')) {
      directory = path.join(output, mode, 'dist');
      if (!fs.existsSync(directory)) continue;
      const live = mode[0] === '1', settings = mode[1] === '1';
      const context = await browser.newContext({ viewport: { width: 1280, height: 900 }, colorScheme: 'dark' });
      const page = await context.newPage(), errors = [], requests = [], writes = [];
      let otaActive = false, otaEnabled = true, protectedAccess = !settings;
      let capabilitiesReads = 0, releaseSafety;
      const safetyGate = new Promise(resolve => { releaseSafety = resolve; });
      let delayNextPoll = false, delayedPollStarted, releaseDelayedPoll;
      const pollStarted = new Promise(resolve => { delayedPollStarted = resolve; });
      const pollGate = new Promise(resolve => { releaseDelayedPoll = resolve; });
      page.on('pageerror', error => errors.push(error.message));
      page.on('dialog', dialog => dialog.accept());
      await page.addInitScript(() => {
        window.__sockets = []; window.__subscriptions = [];
        class Socket {
          static CONNECTING = 0; static OPEN = 1; static CLOSED = 3;
          constructor(url) { this.url = url; this.readyState = 0; window.__sockets.push(this); setTimeout(() => { this.readyState = 1; this.onopen?.({}); }, 5); }
          send(text) { if (text === '__pong') return; const value = JSON.parse(text); window.__subscriptions.push(value); setTimeout(() => this.onmessage?.({ data: JSON.stringify({ uptime: 1000, system: { otaActive: false, otaHasPassword: true }, sensor: { value: 42 }, _runtimeGroups: value.groups || [] }) }), 5); }
          close() { this.readyState = 3; this.onclose?.({}); }
        }
        window.WebSocket = Socket;
      });
      await page.route(`${base}/**`, async route => {
        const request = route.request(), url = new URL(request.url());
        if (url.pathname === '/' || url.pathname.startsWith('/assets/') || url.pathname.endsWith('.svg')) return route.continue();
        requests.push(url.pathname);
        const json = (value, status = 200) => route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(value) });
        if (url.pathname === '/ui_capabilities') {
          if (++capabilitiesReads === 1) return json({}); // Malformed safety data must not unlock controls.
          await safetyGate;
          const status = { live, settings, otaEnabled, otaActive, otaHasPassword: true, settingsAuthRequired: protectedAccess };
          if (delayNextPoll) { delayNextPoll = false; delayedPollStarted(); await pollGate; }
          return json(status);
        }
        if (url.pathname === '/appinfo') return json({ appName: 'UI feature fixture', appTitle: 'UI feature fixture', version: '4.9.0', guiLogging: live });
        if (url.pathname === '/version') return json({ version: '4.9.0' });
        if (url.pathname === '/user_theme.css') return route.fulfill({ contentType: 'text/css', body: '.feature-value{color:rgb(17,34,51)}' });
        if (url.pathname === '/config/auth') {
          const password = request.postDataJSON()?.password || '';
          if (protectedAccess && password !== 'access') return json({ status: 'error', reason: 'unauthorized' }, 403);
          return json({ status: 'ok', token: 'fixture-token', ttlSec: 300 });
        }
        if (url.pathname === '/config/password' && settings) return json({ status: 'ok', value: 'fixture-ota' });
        if (url.pathname === '/config.json' && settings) return json({ Control: { threshold: { value: 42, displayName: 'Threshold', key: 'threshold' } } });
        if (url.pathname.startsWith('/config/') && settings) { writes.push(url.pathname); return json({ status: 'ok' }); }
        if (url.pathname === '/live_pages.json' && live) return json({ pageScoped: true, hasLiveContent: true, pages: [{ key: 'control', name: 'Control', title: 'Control', order: 1 }], defaultPage: 'control' });
        if (url.pathname === '/live_layout.json' && live) return json({ pages: [{ name: 'Control', title: 'Control', key: 'control', cards: [{ name: 'Values', title: 'Values', items: ['value'], groups: [] }] }], placements: {}, defaultPage: 'Control' });
        if (url.pathname === '/runtime_meta.json' && live) return json([{ key: 'value', label: 'Measured value', sourceGroup: 'sensor', group: '', page: 'Control', card: 'Values', precision: 0 }]);
        if (url.pathname === '/runtime.json' && live) return json({ uptime: 1000, sensor: { value: 42 }, system: { otaActive, otaHasPassword: true } });
        if (url.pathname === '/gui/action' && live) { writes.push(url.pathname); return json({ status: 'ok' }); }
        if (url.pathname === '/ota_update') {
          writes.push('/ota_update');
          return json({ status: 'error', reason: 'unauthorized' }, 401);
        }
        return json({ error: 'disabled route' }, 404);
      });
      try {
      await page.goto(base);
      await page.getByRole('heading', { name: /UI feature fixture/ }).waitFor();
      const flash = page.getByRole('button', { name: 'Flash', exact: true });
      await flash.waitFor();
      assert.equal(await flash.isDisabled(), true, mode + ' starts fail-closed');
      if (settings) assert.equal(await page.getByRole('button', { name: 'Settings', exact: true }).isDisabled(), true);
      assert(!requests.some(name => /^\/(?:runtime|live_)/.test(name)), mode + ' must await first valid safety status');
      releaseSafety();
      await page.waitForFunction(() => [...document.querySelectorAll('button')].some(button => button.textContent.trim() === 'Flash' && !button.disabled));
      assert.equal(await page.locator('#user-theme-css').count(), 1, mode + ' custom CSS');
      assert.equal(await page.evaluate(() => document.documentElement.dataset.theme), 'dark');
      assert.equal(await page.evaluate(() => getComputedStyle(document.body).backgroundColor), 'rgb(13, 17, 23)', mode + ' shared dark theme');
      if (settings) {
        await page.getByRole('button', { name: 'Settings', exact: true }).click();
        await page.getByText('Threshold:', { exact: true }).first().waitFor();
        assert(requests.includes('/config.json'));
        await page.getByRole('button', { name: 'Apply All', exact: true }).click();
        await page.waitForFunction(() => [...document.querySelectorAll('*')].some(element => /Applied|applied/.test(element.textContent) && element.children.length === 0));
        assert(writes.includes('/config/apply_all'));
        // Reproduce early Settings reloads while Live discovery is absent or still settling.
        for (let reload = 0; reload < 3; reload++) {
          await page.reload();
          const settingsButton = page.getByRole('button', { name: 'Settings', exact: true });
          await settingsButton.waitFor();
          await page.waitForFunction(() => [...document.querySelectorAll('button')].some(button => button.textContent.trim() === 'Settings' && !button.disabled));
          await settingsButton.click();
          await page.getByText('Threshold:', { exact: true }).first().waitFor();
        }
      } else assert.equal(await page.getByRole('button', { name: 'Settings', exact: true }).count(), 0);
      if (mode === '10') {
        await page.evaluate(() => window.__sockets.find(socket => socket.url.includes('runtime=none')).onmessage({ data: JSON.stringify({ type: 'warningMessage', title: 'Operational warning', message: 'Fixture alarm', actions: [{ id: 'ack', label: 'Acknowledge' }], context: { guiActionEndpoint: '/gui/action', guiMessageId: 'fixture-message' } }) }));
        await page.getByRole('alertdialog', { name: 'Operational warning' }).waitFor();
        await page.getByRole('button', { name: 'Acknowledge', exact: true }).click();
        await page.getByText('Acknowledge succeeded').waitFor();
        assert(writes.includes('/gui/action'));
      }
      await flash.click();
      const auth = page.getByText('OTA Flash Access Required', { exact: true });
      if (await auth.isVisible()) {
        const input = page.locator('input[placeholder="Enter settings password"]');
        await input.fill(protectedAccess ? 'wrong' : '');
        await page.getByRole('button', { name: 'Authenticate', exact: true }).click();
        if (protectedAccess) {
          await page.getByText(/Invalid password/).waitFor();
          await input.fill('access');
          await page.getByRole('button', { name: 'Authenticate', exact: true }).click();
        }
      }
      if (!settings) {
        await page.getByText('OTA Password Required', { exact: true }).waitFor();
        await page.locator('input[placeholder="Enter OTA password"]').fill('wrong-ota');
        await page.getByRole('button', { name: 'Continue', exact: true }).click();
      }
      await page.locator('input[type="file"]').setInputFiles({ name: 'fixture.bin', mimeType: 'application/octet-stream', buffer: Buffer.from([1, 2, 3, 4]) });
      await page.getByText('Unauthorized: wrong OTA password.', { exact: true }).waitFor();
      if (mode !== '00') {
        // An old idle response arriving after the busy event must not release the lock.
        delayNextPoll = true;
        await pollStarted;
      }
      otaActive = true;
      if (mode !== '00') {
        await page.evaluate(() => window.__sockets.find(socket => socket.url.includes('runtime=none')).onmessage({ data: JSON.stringify({ type: 'otaStatus', otaActive: true, otaEnabled: true, otaHasPassword: true }) }));
        await page.getByRole('alertdialog', { name: 'OTA update in progress' }).waitFor({ timeout: 1000 });
        releaseDelayedPoll();
        await page.waitForTimeout(350);
        assert.equal(await page.getByRole('alertdialog', { name: 'OTA update in progress' }).count(), 1, mode + ' stale idle poll must not clear newer busy event');
      }
      await page.waitForFunction(() => [...document.querySelectorAll('button')].find(button => button.textContent.trim() === 'Flash')?.disabled, null, { timeout: 6000 });
      assert.equal(await page.getByRole('alertdialog', { name: 'OTA update in progress' }).count(), 1);
      otaActive = false; otaEnabled = false;
      if (mode !== '00') await page.evaluate(() => window.__sockets.find(socket => socket.url.includes('runtime=none')).onmessage({ data: JSON.stringify({ type: 'otaStatus', otaActive: false, otaEnabled: false, otaHasPassword: true }) }));
      await page.waitForTimeout(2200);
      assert.equal(await flash.isDisabled(), true);
      if (!live) {
        assert(!requests.some(name => /^\/(?:runtime|live_)/.test(name)), mode + ' must not read Live');
        assert.equal(await page.evaluate(() => window.__subscriptions.length), 0);
        assert.equal(await page.evaluate(() => window.__sockets.length), settings ? 1 : 0);
      }
      if (!settings) assert(!requests.some(name => name.startsWith('/config') && name !== '/config/auth'), mode + ' must not read Settings or secrets');
      assert.deepEqual(errors, [], mode + ' runtime errors');
      assert.equal(await page.locator('vite-error-overlay').count(), 0);
      await page.screenshot({ path: path.join(output, mode, 'browser-desktop.png') });
      await page.setViewportSize({ width: 390, height: 844 });
      await page.screenshot({ path: path.join(output, mode, 'browser-mobile.png') });
      results[mode] = { pass: true, title: await page.title(), requests, writes, subscriptions: await page.evaluate(() => window.__subscriptions), errors,
        note: '[MOCKED!] Device responses and sockets; production bundle rendered in installed Chromium.' };
      await context.close();
      console.log(mode, 'browser PASS');
      } catch (error) {
        fs.writeFileSync(path.join(output, mode, 'browser-failure.json'), JSON.stringify({ error: error.message, requests, writes, errors, html: await page.content() }, null, 2));
        await page.screenshot({ path: path.join(output, mode, 'browser-failure.png') });
        throw error;
      }
    }
    fs.writeFileSync(path.join(output, 'browser-results.json'), JSON.stringify(results, null, 2));
  } finally { await browser.close(); server.close(); }
})().catch(error => { console.error(error); server.close(); process.exitCode = 1; });
