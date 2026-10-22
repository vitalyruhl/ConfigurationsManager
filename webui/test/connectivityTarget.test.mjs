import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { parse, compileScript } from '@vue/compiler-sfc';
import { createSSRApp } from 'vue';
import { renderToString } from 'vue/server-renderer';
import { isConnectivityTargetSetting, isConnectivityTargetValue } from '../src/connectivityTarget.mjs';

const componentUrl = new URL('../src/components/Setting.vue', import.meta.url);
const { descriptor } = parse(readFileSync(componentUrl, 'utf8'));
const compiled = compileScript(descriptor, {
  id: 'connectivity-target-test', inlineTemplate: true, templateOptions: { ssr: true },
}).content.replace(/from (["'])([^"']+)\1/g, (_, quote, specifier) => {
  const resolved = specifier.startsWith('.')
    ? new URL(specifier, componentUrl).href : import.meta.resolve(specifier);
  return `from ${JSON.stringify(resolved)}`;
});
const { default: Setting } = await import(`data:text/javascript;base64,${Buffer.from(compiled).toString('base64')}`);

async function render(value, extra = {}) {
  // [MOCKED!] Settings metadata; render the unchanged production Vue component.
  return renderToString(createSSRApp(Setting, {
    category: 'WiFi', keyName: 'hashed_probe',
    settingData: { displayName: 'Connectivity check target', value }, ...extra,
  }));
}

test('one editable target field offers Off and Standard gateway for hashed settings', async () => {
  const html = await render('gateway');
  assert.match(html, /<input[^>]+list="[^"]+"/);
  assert.match(html, /<datalist[^>]*>/);
  assert.match(html, /<option[^>]+value="off"[^>]+label="Off"/);
  assert.match(html, /<option[^>]+value="gateway"[^>]+label="Standard gateway"/);
  assert.equal((html.match(/<input/g) || []).length, 1);
  assert.match(html, /value="gateway"/);
  assert.doesNotMatch(html, /aria-invalid="true"/);
});

test('invalid target syntax disables Apply and Save in the rendered setting', async () => {
  const html = await render('192.168.1.999');
  assert.match(html, /aria-invalid="true"/);
  assert.match(html, /class="apply-btn"[^>]*disabled/);
  assert.match(html, /class="save-btn"[^>]*disabled/);
});

test('a syntactically valid but unreachable address remains saveable', async () => {
  const html = await render('192.0.2.77');
  assert.doesNotMatch(html, /aria-invalid="true"/);
  assert.doesNotMatch(html, /class="(?:apply|save)-btn"[^>]*disabled/);
});


test('target recognition supports the public label and explicit key without changing other settings', () => {
  assert.equal(isConnectivityTargetSetting('WiFi', 'hashed_probe', { name: 'Connectivity check target' }), true);
  assert.equal(isConnectivityTargetSetting('WiFi', 'WiFiProbe'), true);
  assert.equal(isConnectivityTargetSetting('WiFi', 'hashed_probe', { key: 'WiFiProbe' }), true);
  assert.equal(isConnectivityTargetSetting('MQTT', 'WiFiProbe'), false);
  assert.equal(isConnectivityTargetSetting('WiFi', 'WiFiGateway'), false);
});

test('strict target syntax matches the server unicast policy', () => {
  for (const value of ['off', 'gateway', '1.0.0.1', '192.168.2.1', '192.0.2.77', '223.255.255.255']) {
    assert.equal(isConnectivityTargetValue(value), true, value);
  }
  for (const value of ['', 'Off', 'Gateway', ' gateway', 'gateway ', 'example.com', '192.168.01.1',
    '192.168.1.256', '192.168.1', '1.2.3.4.5', '1.2.3.-1', '1.2.3.+4', '1.2.3.4\n',
    '0.0.0.0', '0.1.2.3', '224.0.0.1', '240.0.0.1', '255.255.255.255', '1.2.3.4' + String.fromCharCode(10), null, true, 123]) {
    assert.equal(isConnectivityTargetValue(value), false, String(value));
  }
});

test('unrelated WiFi fields remain ordinary editable inputs', async () => {
  const html = await render('gateway', {
    keyName: 'WiFiGateway', settingData: { displayName: 'Gateway', value: 'gateway' },
  });
  assert.doesNotMatch(html, /<datalist|aria-invalid/);
  assert.doesNotMatch(html, /class="(?:apply|save)-btn"[^>]*disabled/);
});
