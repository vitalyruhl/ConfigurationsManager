import test from 'node:test';
import assert from 'node:assert/strict';
import { formatWiFiPowerSaveStatus } from '../src/wifiPowerSave.mjs';

test('WiFi driver snapshot distinguishes request, actual mode and failed reads', () => {
  const status = { requested: true, effectiveMode: 0, readError: 0, applyAttempted: true, applySucceeded: false };
  assert.match(formatWiFiPowerSaveStatus(status), /requested on; driver off.*last apply failed/);
  assert.match(formatWiFiPowerSaveStatus({ ...status, effectiveMode: 1, applySucceeded: true }), /driver on.*MIN_MODEM/);
  assert.match(formatWiFiPowerSaveStatus({ ...status, effectiveMode: 2 }), /MAX_MODEM/);
  assert.match(formatWiFiPowerSaveStatus({ ...status, effectiveMode: 1, readError: 123 }), /driver unknown.*read error 123/);
  assert.match(formatWiFiPowerSaveStatus({ ...status, effectiveMode: 99 }), /driver unknown/);
  assert.match(formatWiFiPowerSaveStatus({ ...status, applyAttempted: false }), /not attempted/);
  assert.match(formatWiFiPowerSaveStatus(status), /At settings load.*Reload settings/);
  assert.equal(formatWiFiPowerSaveStatus(undefined), '');
});

test('BLE override is explicit while an ordinary requested ON is not mislabeled', () => {
  const status = { requested: false, effectiveMode: 1, readError: 0, applyAttempted: true, applySucceeded: true, applyError: 0, bleCoexistenceRequired: true, reason: 'ble_coexistence' };
  assert.match(formatWiFiPowerSaveStatus(status), /requested off; driver on.*BLE coexistence.*until restart.*saved preference is unchanged/);
  assert.doesNotMatch(formatWiFiPowerSaveStatus({ ...status, requested: true, reason: 'requested' }), /BLE coexistence requires/);
});
