# Rename plan

## Ordered rename list

1. `ConfigManagerWiFi::setWifiAPMacFilter(const String& macAddress)` → `ConfigManagerWiFi::setAccessPointMacFilter(const String& macAddress)`
   - Rationale: remove redundant subsystem token in method name and keep concept wording consistent.
   - Rule refs: E, D.
   - Impacted files:
     - `src/wifi/WiFiManager.h`
     - `src/wifi/WiFiManager.cpp`
     - `src/ConfigManager.h`
     - `examples/BME280-Temp-Sensor/src/main.cpp`
     - `examples/BoilerSaver/src/main.cpp`
     - `examples/Full-GUI-Demo/src/main.cpp`
     - `examples/Full-Logging-Demo/src/main.cpp`
     - `examples/Full-IO-Demo/src/main.cpp`
     - `examples/minimal/src/main.cpp`
     - `examples/SolarInverterLimiter/src/main.cpp`
     - `docs/WIFI.md`

2. `ConfigManagerWiFi::setWifiAPMacPriority(const String& macAddress)` → `ConfigManagerWiFi::setAccessPointMacPriority(const String& macAddress)`
   - Rationale: same naming normalization as MAC filter.
   - Rule refs: E, D.
   - Impacted files: same set as above except filter-only references.

3. `ConfigManagerWiFi::performWiFiStackReset()` → `ConfigManagerWiFi::performStackReset()`
   - Rationale: remove redundant subsystem identifier in method name.
   - Rule refs: E.
   - Impacted files:
     - `src/wifi/WiFiManager.h`
     - `src/wifi/WiFiManager.cpp`

## Status
- 1) APPLIED
- 2) APPLIED
- 3) APPLIED
