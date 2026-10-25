// Driver values are snapshots from Settings JSON, independent of the Live UI.
export function formatWiFiPowerSaveStatus(status) {
  if (!status || typeof status.requested !== 'boolean') return '';
  const requested = status.requested ? 'on' : 'off';
  const modes = { 0: 'off (NONE)', 1: 'on (MIN_MODEM)', 2: 'on (MAX_MODEM)' };
  const effective = status.readError === 0 && Object.hasOwn(modes, status.effectiveMode)
    ? modes[status.effectiveMode] : 'unknown';
  const applied = !status.applyAttempted ? 'not attempted'
    : status.applySucceeded ? 'succeeded' : 'failed';
  const applyError = status.applyAttempted && !status.applySucceeded ? `; apply error ${status.applyError ?? 'unknown'}` : '';
  const coexistence = status.reason === 'ble_coexistence' ? ' BLE coexistence requires MIN_MODEM until restart; the saved preference is unchanged.' : '';
  const error = status.readError === 0 ? '' : `; read error ${status.readError ?? 'unknown'}`;
  return `At settings load: requested ${requested}; driver ${effective}; last apply ${applied}${applyError}${error}.${coexistence} Reload settings for a new snapshot.`;
}
