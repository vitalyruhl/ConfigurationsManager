# WiFi

This document collects WiFi-related usage patterns and best practices for ConfigurationsManager.

## Topics

- Station mode (DHCP / static IP)
- AP mode fallback
- Settings-driven default behavior (`startWebServer()`)
- Global WiFi hooks (`onWiFiConnected/onWiFiDisconnected/onWiFiAPMode`)
- OTA + NTP patterns
- Smart WiFi Roaming (mesh / multi-AP)
- AP selection helpers (MAC filter / priority)
- Reconnect + auto reboot timeout

## Quickstart (recommended)

If you use the built-in core settings templates, you can keep your sketch very small:

1. Register core WiFi + system settings
2. Load settings from NVS (`loadAll()`)
3. Start WiFi using the settings-driven overload `startWebServer()`
4. Implement optional global WiFi hooks for OTA/NTP/your services

Example (based on examples/IO-Full-Demo):

```cpp
#include "ConfigManager.h"
#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"

static cm::CoreSettings& coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings& systemSettings = coreSettings.system;
static cm::CoreNtpSettings& ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

void setup() {
  ConfigManager.setAppName("MyDevice");
  coreSettings.attachWiFi(ConfigManager);
  coreSettings.attachSystem(ConfigManager);
  coreSettings.attachNtp(ConfigManager);

  ConfigManager.loadAll();
  ConfigManager.startWebServer();

  // Optional: reboot if WiFi stays down too long.
  ConfigManager.getWiFiManager().setAutoRebootTimeout((unsigned long)systemSettings.wifiRebootTimeoutMin.get());
}

void onWiFiConnected() {
  wifiServices.onConnected(ConfigManager, "MyDevice", systemSettings, ntpSettings);
}

void onWiFiDisconnected() {
  wifiServices.onDisconnected();
}

void onWiFiAPMode() {
  wifiServices.onAPMode();
}
```

## Settings (Core WiFi bundle)

If you use `cm::CoreSettings` (see src/core/CoreSettings.h), the following settings exist under category `WiFi`:

| Key | Name (UI) | Type | Meaning |
|---|---|---|---|
| `WiFiSSID` | WiFi SSID | string | STA SSID (empty → AP fallback) |
| `WiFiPassword` | WiFi Password | password | STA password |
| `WiFiUseDHCP` | Use DHCP | bool | `true` = DHCP, `false` = static IP |
| `WiFiStaticIP` | Static IP | string | Static IP address |
| `WiFiGateway` | Gateway | string | Default gateway |
| `WiFiSubnet` | Subnet Mask | string | Subnet mask |
| `WiFiDNS1` | Primary DNS | string | Optional DNS1 |
| `WiFiDNS2` | Secondary DNS | string | Optional DNS2 |

Related system settings (category `System`):

| Key | Name (UI) | Type | Meaning |
|---|---|---|---|
| `OTAEn` | Allow OTA Updates | bool | Enables ArduinoOTA (and OTA web routes) |
| `OTAPass` | OTA Password | password | Password used for ArduinoOTA |
| `WiFiRb` | Reboot if WiFi lost (min) | int | Auto reboot timeout in minutes |

Important ESP32 note:

- ESP32 has a 15 character limitation for internal preference keys.
- ConfigManager builds a storage key from `<category>_<key>`.
- Keep categories short (or use the core bundles which are already compatible).

## Station mode (DHCP vs. static IP)

ConfigurationsManager provides overloads to start WiFi + web server:

- DHCP:
  - `startWebServer(ssid, password)`
- Static IP:
  - `startWebServer(staticIP, gateway, subnet, ssid, password, dns1, dns2)`

If you prefer explicit control (as shown in examples/example_min), you can do:

```cpp
const String ssid = wifiSettings.wifiSsid.get();
const String password = wifiSettings.wifiPassword.get();

if (ssid.isEmpty()) {
  ConfigManager.startAccessPoint();
  return;
}

if (wifiSettings.useDhcp.get()) {
  ConfigManager.startWebServer(ssid, password);
} else {
  IPAddress ip, gw, mask, dns1, dns2;
  ip.fromString(wifiSettings.staticIp.get());
  gw.fromString(wifiSettings.gateway.get());
  mask.fromString(wifiSettings.subnet.get());

  const String dns1Str = wifiSettings.dnsPrimary.get();
  const String dns2Str = wifiSettings.dnsSecondary.get();
  if (!dns1Str.isEmpty()) dns1.fromString(dns1Str);
  if (!dns2Str.isEmpty()) dns2.fromString(dns2Str);

  ConfigManager.startWebServer(ip, gw, mask, ssid, password, dns1, dns2);
}
```

## Settings-driven default behavior (auto)

`ConfigManager.startWebServer()` (no args) reads the persisted/registered WiFi settings (category `WiFi`) and selects:

- DHCP vs. static based on `WiFiUseDHCP`
- AP fallback if `WiFiSSID` is empty or the settings are not registered
- If static IP fields are invalid, it falls back to DHCP

This is the preferred pattern if you use the core bundles.

## AP Mode fallback

If no SSID is configured, starting an AP is a common fallback.

Typical flow:

- if SSID is empty → `startAccessPoint()`
- if the device runs in AP mode → skip normal web server startup

Notes:

- `startAccessPoint()` uses `<appName>_AP` by default.
- Empty AP password means open network.

## Global WiFi hooks

ConfigManager calls these global functions on WiFi state changes:

- `onWiFiConnected()`
- `onWiFiDisconnected()`
- `onWiFiAPMode()`

If you do not provide them, the library provides weak no-op defaults (see src/default_hooks.cpp).

Use these hooks to start/stop your services (OTA, NTP, MQTT, sensors, etc.).

## OTA + NTP patterns

### OTA

OTA is configured via the system settings bundle:

- `systemSettings.allowOTA` (key `OTAEn`)
- `systemSettings.otaPassword` (key `OTAPass`)

Typical pattern:

- initialize OTA once on first connect
- keep calling `ConfigManager.handleOTA()` in `loop()`

The helper in src/core/CoreWiFiServices.h does exactly that (and also manages NTP).

### NTP

The core NTP bundle provides:

- `NTPFrq` (sync interval in seconds)
- `NTP1`/`NTP2` (servers)
- `NTPTZ` (POSIX TZ)

The helper uses `configTzTime()` and schedules periodic resync via `Ticker`.

## Reconnect + auto reboot timeout

If WiFi is lost:

- call `reconnectWifi()`
- wait briefly
- re-initialize OTA (if you rely on ArduinoOTA)

In most cases you should *not* implement your own reconnect loop.
Instead, call these regularly in `loop()`:

```cpp
ConfigManager.getWiFiManager().update();
ConfigManager.handleClient();
ConfigManager.handleOTA();
```

Auto reboot timeout:

- `ConfigManager.getWiFiManager().setAutoRebootTimeout(minutes)`
- If the device stays disconnected long enough, it triggers a reboot (useful for unattended devices).

Important notes:

- Avoid tight reconnect loops.
- Prefer backoff / cooldown logic if your environment has unstable WiFi.

## Smart WiFi Roaming

Smart WiFi Roaming is documented in its own file:

- `docs/SMART_ROAMING.md`

That document covers:

- how roaming decisions are made
- how to tune thresholds/cooldowns
- how to disable roaming

Quick configuration example (see examples/IO-Full-Demo):

```cpp
ConfigManager.enableSmartRoaming(true);
ConfigManager.setRoamingThreshold(-75);
ConfigManager.setRoamingCooldown(30);
ConfigManager.setRoamingImprovement(10);
```

## AP selection helpers (MAC filter / priority)

You can influence which AP is preferred/allowed (useful for multi-AP setups):

```cpp
// Only connect to this specific AP MAC
ConfigManager.setWifiAPMacFilter("60:B5:8D:4C:E1:D5");

// Prefer this AP MAC (fallback to others)
ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");
```

## ADC2 warning (ESP32)

On classic ESP32 (Arduino), ADC2 pins are typically not usable for `analogRead()` while WiFi is active.

If you use analog inputs in parallel with WiFi:

- prefer ADC1 pins (GPIO32–39)
- see also `docs/esp.md` and `docs/IO-AnalogInputs.md`
