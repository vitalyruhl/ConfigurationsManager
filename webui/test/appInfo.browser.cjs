// [MOCKED!] Run the actual embedded page with all device HTTP and sockets intercepted.
// PLAYWRIGHT_MODULE and PLAYWRIGHT_BROWSER_PATH select an existing local runtime.
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');
const assert = require('node:assert/strict');
const header = fs.readFileSync(path.join(__dirname, '../../src/html_content.h'), 'utf8');
const array = header.split('WEB_HTML_GZ[] PROGMEM = {')[1].split('};')[0];
const html = zlib.gunzipSync(Buffer.from([...array.matchAll(/0x([0-9a-f]{2})/gi)].map(m => parseInt(m[1], 16))));
(async () => {
  const browser = await chromium.launch({ executablePath: process.env.PLAYWRIGHT_BROWSER_PATH || undefined, headless: true });
  try {
    const page = await browser.newPage();
    const errors = [];
    let appCalls = 0, versionCalls = 0, offline = false;
    page.on('pageerror', error => errors.push(error.message));
    await page.addInitScript(() => {
      window.__testSockets = [];
      window.WebSocket = class {
        static CONNECTING = 0; static OPEN = 1; static CLOSED = 3;
        constructor(url) { this.url = url; this.readyState = 0; window.__testSockets.push(this); setTimeout(() => { this.readyState = 1; this.onopen?.({}); }, 10); }
        send() {}
        close() { this.readyState = 3; }
      };
    });
    await page.route('**/*', async route => {
      const url = new URL(route.request().url());
      const json = body => route.fulfill({ contentType: 'application/json', body: JSON.stringify(body) });
      if (url.pathname === '/') return route.fulfill({ contentType: 'text/html', body: html });
      if (url.pathname === '/appinfo') {
        appCalls++;
        if (appCalls === 1 || offline) return route.abort('connectionreset');
        return json({ appName: 'SolarBatteryTelemetry', appTitle: 'Battery Monitor', version: '1.0.0', guiLogging: false });
      }
      if (url.pathname === '/version') {
        versionCalls++;
        return offline ? route.abort('connectionreset') : route.fulfill({ contentType: 'text/plain', body: 'SolarBatteryTelemetry 1.0.0' });
      }
      if (url.pathname === '/live_pages.json') return json({ pageScoped: true, hasLiveContent: false, pages: [], defaultPage: '' });
      if (url.pathname === '/ui_capabilities') return json({ live: true, settings: true, otaEnabled: false, otaActive: false, otaHasPassword: true, settingsAuthRequired: false });
      if (url.pathname === '/user_theme.css') return route.fulfill({ contentType: 'text/css', body: '' });
      if (url.pathname === '/runtime.json') return json({ system: { otaActive: false, otaHasPassword: true } });
      return json({});
    });
    await page.goto('http://cm-appinfo.test/');
    await page.locator('#mainHeader').filter({ hasText: 'SolarBatteryTelemetry 1.0.0' }).waitFor();
    assert.equal(versionCalls, 1, 'network failure must use legacy version');
    await page.waitForFunction(() => document.title === 'Battery Monitor 1.0.0', null, { timeout: 7000 });
    assert.equal(appCalls, 2, 'one bounded retry must restore structured identity');
    offline = true;
    const fallbackFailed = page.waitForEvent('requestfailed', {
      predicate: request => new URL(request.url()).pathname === '/version', timeout: 5000,
    });
    await page.evaluate(() => window.__testSockets.find(socket => socket.url.includes('runtime=none')).onopen({}));
    await fallbackFailed;
    assert.equal(versionCalls, 2, 'reconnect failure must also use fallback');
    assert.equal(await page.locator('#mainHeader').innerText(), 'SolarBatteryTelemetry 1.0.0');
    assert.equal(await page.title(), 'Battery Monitor 1.0.0');
    assert.deepEqual(errors, []);
    console.log('PASS embedded WebUI: network fallback, post-reboot retry, retained identity after reconnect failure.');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
