# Naming inconsistencies audit

## Scope
- Source roots: `src/`
- Examples roots: `examples/`
- Documentation root: `docs/`
- Public API baseline: headers in `src/**/*.h` (repository has no `include/` directory).

rg pattern used for focused audit:
- `setWifiAPMacFilter|setWifiAPMacPriority|performWiFiStackReset`

| Symbol | Signature | Location | Current name | Proposed name | Rule violated | Notes |
|---|---|---|---|---|---|---|
| ConfigManagerWiFi::setWifiAPMacFilter | `void setWifiAPMacFilter(const String& macAddress)` | `src/wifi/WiFiManager.h` | `setWifiAPMacFilter` | `setAccessPointMacFilter` | E (subsystem context redundancy), D (concept/acronym consistency) | `WiFi` token is redundant in `ConfigManagerWiFi` scope. |
| ConfigManagerWiFi::setWifiAPMacPriority | `void setWifiAPMacPriority(const String& macAddress)` | `src/wifi/WiFiManager.h` | `setWifiAPMacPriority` | `setAccessPointMacPriority` | E (subsystem context redundancy), D (concept/acronym consistency) | Same concept family as MAC filter method. |
| ConfigManagerWiFi::performWiFiStackReset | `void performWiFiStackReset()` | `src/wifi/WiFiManager.h` | `performWiFiStackReset` | `performStackReset` | E (subsystem context redundancy) | Owning subsystem already expresses WiFi context. |
| ConfigManagerClass::setWifiAPMacFilter | `void setWifiAPMacFilter(const String& macAddress)` | `src/ConfigManager.h` | `setWifiAPMacFilter` | `setAccessPointMacFilter` | E (subsystem context redundancy), D (concept/acronym consistency) | Wrapper API must mirror renamed backend method. |
| ConfigManagerClass::setWifiAPMacPriority | `void setWifiAPMacPriority(const String& macAddress)` | `src/ConfigManager.h` | `setWifiAPMacPriority` | `setAccessPointMacPriority` | E (subsystem context redundancy), D (concept/acronym consistency) | Wrapper API must mirror renamed backend method. |
