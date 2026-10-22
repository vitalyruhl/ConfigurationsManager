// Match the single WiFi target setting even when JSON uses its hashed storage key.
export function isConnectivityTargetSetting(category, keyName, settingData = {}) {
  if (String(category).toLowerCase() !== 'wifi') return false;
  return [keyName, settingData.key, settingData.displayName, settingData.name]
    .some(value => value === 'WiFiProbe' || value === 'Connectivity check target');
}

export function isConnectivityTargetValue(value) {
  if (value === 'off' || value === 'gateway') return true;
  if (typeof value !== 'string') return false;
  const octets = value.split('.');
  if (octets.length !== 4 || !octets.every(octet =>
    /^(0|[1-9][0-9]{0,2})$/.test(octet) && Number(octet) <= 255)) return false;
  // Keep the same unicast range as cm::detail::parseConnectivityTarget.
  return Number(octets[0]) > 0 && Number(octets[0]) < 224;
}
