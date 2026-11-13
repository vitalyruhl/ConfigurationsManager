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
- Power-saving preferences and BLE coexistence

## Quickstart (recommended)

If you use the built-in core settings templates, you can keep your sketch very small:

1. Register core WiFi + system settings
2. Load settings from NVS (`loadAll()`)
3. Start WiFi using the settings-driven overload `startWebServer()`
4. Implement optional global WiFi hooks for OTA/NTP/your services

Example (based on examples/Full-IO-Demo):

```cpp
#include "ConfigManager.h"
#include "core/CoreSettings.h"
 #include "core/CoreWiFiServices.h"
 
 static cm::CoreSettings& coreSettings = cm::CoreSettings::instance();
 static cm::CoreWiFiSettings& wifiSettings = coreSettings.wifi;
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
 
   // startWebServer() applies the registered WiFi recovery settings.
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
| `WiFiSSID` | WiFi SSID | string | STA SSID (empty -> AP fallback) |
| `WiFiPassword` | WiFi Password | password | STA password |
| `WiFiUseDHCP` | Use DHCP | bool | `true` = DHCP, `false` = static IP |
| `WiFiStaticIP` | Static IP | string | Static IP address |
| `WiFiGateway` | Gateway | string | Default gateway |
| `WiFiSubnet` | Subnet Mask | string | Subnet mask |
| `WiFiDNS1` | Primary DNS | string | Optional DNS1 |
| `WiFiDNS2` | Secondary DNS | string | Optional DNS2 |
| `WiFiPowerSave` | WiFi power saving | bool | Saved modem power-saving preference; default false |
| `WiFiProbe` | Connectivity check target | string | `off` (default), `gateway`, or strict unicast IPv4 |
| `WiFiRb` | Reboot if WiFi lost (min) | int | Whole minutes 0..9999; default 1; 0 disables network restarts |

Related system settings (category `System`):

| Key | Name (UI) | Type | Meaning |
|---|---|---|---|
| `OTAEn` | Allow OTA Updates | bool | Enables ArduinoOTA (and OTA web routes) |
| `OTAPass` | OTA Password | password | Password used for ArduinoOTA |

Important ESP32 note:
 
 - ESP32 has a 15 character limitation for internal preference keys.
 - `ConfigOptions.key` is the preference key. If it is `nullptr`, ConfigManager auto-generates a key from category + name.
 - Keep categories short (or use the core bundles which are already compatible).

## Station mode (DHCP vs. static IP)

ConfigurationsManager provides overloads to start WiFi + web server:

- DHCP:
  - `startWebServer(ssid, password)`
- Static IP:
  - `startWebServer(staticIP, gateway, subnet, ssid, password, dns1, dns2)`

If you prefer explicit control (as shown in examples/minimal), you can do:

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

 - if SSID is empty -> `startAccessPoint()`
 - if the device runs in AP mode -> skip normal web server startup

Notes:

- `startAccessPoint()` uses `<appName>_AP` by default.
- Empty AP password means open network.
- In AP/setup mode, minimal captive-portal compatibility redirects send
  `/generate_204`, `/canonical.html`, and guarded unknown browser navigation
  requests back to the Web UI root. STA/runtime mode keeps normal 404 behavior.

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

## Connectivity checks and independent loop recovery

With `WiFiProbe=off`, WiFi-loss recovery stays active. An enabled target also
requires matching ICMP replies every ten seconds, with a one-second deadline;
`gateway` follows the currently active DHCP/static gateway. A connected radio
alone does not keep an unreachable target healthy. AP mode and OTA suspend
network-triggered restarts and grant a fresh recovery window afterward.

`WiFiRb` is still stored in minutes: 60/120 provide one/two hours. Its one-minute
default applies to new/reset values; saved values are preserved. Apply changes
RAM, Save changes NVS, and reset/load synchronize the loop-owned recovery state.
Syntax validation does not prove reachability: test with Apply before Save. An
unreachable saved target can cause repeated recovery restarts; retain USB/AP
access to restore `off` or a correct target.

For temporary application overrides, `setConnectivityCheckTarget(target)` and
`setWiFiAutoRebootTimeout(minutes)` validate and queue updates for the servicing
loop; they do not save settings. Continue calling `handleClient()`.

A separate supervisor starts with the first `handleClient()` call and defaults
to 30 seconds. `setLoopWatchdogTimeout(ms)` returns false for unsupported values;
use 0 to disable or 1000..3600000 ms to configure it. It does not install a global
task watchdog and still requires the scheduler/ESP timer task to run. Synchronous
ArduinoOTA progress can confirm loop work; asynchronous HTTP traffic cannot.
Asynchronous HTTP traffic cannot establish application-loop progress. The loop
supervisor still depends on the scheduler and ESP timer task, so it does not
replace interrupt or hardware watchdogs. Build/host checks do not establish
device timing or long-term stability.

## Power saving and BLE coexistence

WiFi defaults to modem power saving off (`WIFI_PS_NONE`). With the core WiFi
bundle attached, `cm::CoreWiFiSettings::powerSave` exposes the boolean setting
with key hint `WiFiPowerSave`. Save persists the choice. The switch displays
that configured choice, while its diagnostic text is a separate runtime snapshot
of the requested state, native driver mode and last apply/read result. Reload
Settings for a fresh snapshot. `/config.json` includes these diagnostics even
with `CM_ENABLE_LIVE_UI=0`; a failed driver read reports an unknown mode.

Preference precedence and lifecycle:

1. `CM_WIFI_POWER_SAVE_DEFAULT` accepts `0` (default) or `1` (MIN_MODEM). It seeds
   a missing setting once; an existing NVS value takes precedence. Without the
   core bundle, it is the WiFi manager's fallback.
2. Loaded settings apply before the first station connection. Reconnects and
   full stack resets reapply the requested policy.
3. `ConfigManager.setWiFiPowerSave(bool)` queues a non-persistent runtime
   override. The latest queued API or Settings update wins when `handleClient()`
   runs. The override does not change the saved switch; loading/resetting Settings
   queues their values again, and reboot restores the saved choice.

Call the queued ConfigManager API from callbacks/tasks. The low-level
`getWiFiManager().setPowerSave(bool)` belongs to the application loop.
`getWiFiManager().getPowerSaveDiagnostics()` is read-only and exposes `requested`,
`bleCoexistenceRequired`, `effectiveMode` (`-1` on read failure), `readError`,
`applyAttempted`, `applySucceeded` and `applyError`. A request before station
startup remains pending until a connection path applies it. CM synchronizes
Arduino's cached sleep preference and explicitly calls `esp_wifi_set_ps` because
the native driver can revert after reset even when the cache matches. A changed
mode can therefore need two SDK calls; the explicit native result determines
apply success.

Before CM starts BLE scanning or GATT work, its worker queues a coexistence lease
and waits for the application loop to acknowledge native `WIFI_PS_MIN_MODEM`.
ESP32/ESP-IDF 4.4.7 does not support `WIFI_PS_NONE` with WiFi/Bluetooth coexistence;
see [Espressif's power-saving constraints](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-guides/wifi.html#esp32-wi-fi-power-saving-mode).
A previous MIN read does not acknowledge a new lease. Apply/read failure leaves
BLE startup pending and retries from the application loop at most once per
second, outside a stack reset. When WiFi is explicitly uninitialized, startup
may proceed after synchronizing Arduino's cache to MIN. Later station starts
and resets reapply the lease.

The saved preference remains unchanged. While the lease is active, requested
false gives effective MIN with `reason=ble_coexistence`; requested true gives
`reason=requested`. Settings JSON exposes `bleCoexistenceRequired` separately;
an applied coexistence override is not an apply error. Ordinary BLE pause,
disable, disconnect, peer removal or cancelled startup does not shut down the
controller or release the lease. The terminal
[BLE restart shutdown](BLUETOOTH.md#terminal-shutdown-for-http-ota-restart) also
retains it conservatively until reboot. The Bluetooth Enabled switch alone does
not prove controller shutdown. WiFi-only applications retain their requested
policy, including the default NONE.

These policy rules do not establish the cause of an observed panic, MQTT stall
or TCP outage. Validate RF behavior and reconnect recovery on the target device.

## Smart WiFi Roaming

Smart WiFi Roaming is documented in its own file:

- `docs/SMART_ROAMING.md`

That document covers:

- how roaming decisions are made
- how to tune thresholds/cooldowns
- how to disable roaming

Quick configuration example (see examples/Full-IO-Demo):

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
ConfigManager.setAccessPointMacFilter("60:B5:8D:4C:E1:D5");

// Prefer this AP MAC (fallback to others)
ConfigManager.setAccessPointMacPriority("60:B5:8D:4C:E1:D5");
```

## Advanced troubleshooting: full stack reset

For unstable environments (driver state issues, repeated reconnect failures), you can trigger a full WiFi stack reset:

```cpp
// Diagnostic fallback (example only)
ConfigManager.performStackReset();
```

Recommended use:

- keep normal reconnect logic as default
- trigger `performStackReset()` only after repeated failures
- keep logs enabled while testing this path

Recovery uses Arduino's `WiFi.mode(WIFI_OFF)` and `WiFi.mode(WIFI_STA)` lifecycle,
so Arduino applies its runtime `WiFi.useStaticBuffers()` preference and initial
TX/RX buffer policy. CM does not mix direct native stop/deinit/init calls into
this path. A failed OFF/STA transition aborts that attempt without reporting
completion; later attempts follow the existing reconnect cadence. This lifecycle
does not prove that a specific outage or panic has been repaired.

## ADC2 warning (ESP32)

On classic ESP32 (Arduino), ADC2 pins are typically not usable for `analogRead()` while WiFi is active.

If you use analog inputs in parallel with WiFi:

- prefer ADC1 pins (GPIO32–39)
- see also [IO analog inputs](IO-AnalogInputs.md)

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `ConfigManager.startWebServer` | `startWebServer()`<br>`startWebServer(const String& ssid, const String& password)`<br>`startWebServer(const IPAddress& staticIP, const IPAddress& gateway, const IPAddress& subnet, const String& ssid, const String& password, const IPAddress& dns1 = IPAddress(), const IPAddress& dns2 = IPAddress())` | Starts WiFi + web server with settings, DHCP, or static IP. | Preferred entry point for most sketches. |
| `ConfigManager.startAccessPoint` | `startAccessPoint(const String& apSSID = "", const String& apPassword = "")` | Starts AP mode fallback. | Used when no STA credentials are available. |
| `ConfigManager.reconnectWifi` | `reconnectWifi()` | Triggers a reconnect attempt for STA mode. | Keep loop non-blocking and combine with `update()`. |
| `ConfigManager.performStackReset` | `performStackReset()` | Performs full WiFi stack re-initialization. | Advanced troubleshooting only. |
| `ConfigManager.setWiFiPowerSave` | `setWiFiPowerSave(bool enabled)` | Queues a runtime power-saving preference. | Non-persistent; service `handleClient()`. |
| `ConfigManager.getWiFiManager().getPowerSaveDiagnostics` | `getPowerSaveDiagnostics()` | Reads requested/effective state and apply/read results. | Snapshot only; does not change the radio. |
| `ConfigManager.setAccessPointMacFilter` | `setAccessPointMacFilter(const String& macAddress)` | Restricts connection to one AP MAC. | Multi-AP tuning helper. |
| `ConfigManager.setAccessPointMacPriority` | `setAccessPointMacPriority(const String& macAddress)` | Prefers one AP MAC but allows fallback APs. | Multi-AP tuning helper. |

