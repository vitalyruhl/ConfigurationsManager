# Settings & Configuration API

This document describes how to declare, register, persist and display configuration values using ConfigurationsManager.

## Core Concepts

- Config<T>: Lightweight wrapper around a persisted value (Preferences / NVS).
- ConfigOptions<T>: Aggregate initialization structure used to construct a Config<T>.
- Registration: Add each Config<T> to the manager via addSetting().
- Persistence: loadAll() on startup, saveAll() (or individual save) after changes.
- Visibility: showIf predicate determines whether a setting appears in the web UI JSON.

## Declaring a Setting (v2.7+)

```cpp
Config<int> sample(ConfigOptions<int>{
  .key = "interval",
  .name = "Update Interval (s)",
  .category = "main",
  .defaultValue = 30,
  .showInWeb = true,
  .callback = [](){ Serial.println("Interval changed"); }
});
```

Add to manager and load persisted value:

```cpp
cfg.addSetting(&sample);
cfg.loadAll();
```

### Key Length & Truncation Safety

Internal storage key format: `<category>_<key>` truncated to 15 chars to satisfy ESP32 NVS limits. Provide human‑friendly `.name` / `.categoryPretty` for UI text. Avoid relying on the raw key for user output.

### Password / Secret Fields

Set `.isPassword = true` to mask in the UI. The backend stores the real value; the UI obscures it and only sends a new value when the field changes.


## Reducing Boilerplate with OptionGroup (since 2.4.3)

When many settings share the same category and pretty category, you can use `OptionGroup` to avoid repetition.

Before (direct ConfigOptions):

```cpp
Config<String> wifiSsid(ConfigOptions<String>{
  .key = "ssid", .name = "WiFi SSID", .category = "wifi", .defaultValue = "MyWiFi",
  .categoryPretty = "Network Settings"
});
Config<String> wifiPassword(ConfigOptions<String>{
  .key = "password", .name = "WiFi Password", .category = "wifi", .defaultValue = "secretpass",
  .categoryPretty = "Network Settings", .showInWeb = true, .isPassword = true
});
```

After (factory pattern):

```cpp
constexpr OptionGroup WIFI_GROUP{"wifi", "Network Settings"};

Config<String> wifiSsid( WIFI_GROUP.opt<String>(
  "ssid", String("MyWiFi"), "WiFi SSID") );

Config<String> wifiPassword( WIFI_GROUP.opt<String>(
  "password", String("secretpass"), "WiFi Password", true, true) );
```

The template `opt<T>(key, defaultValue, name, showInWeb, isPassword, callback, showIf)` returns a fully populated `ConfigOptions<T>`.


Use an OptionGroup when many settings share the same category and pretty category:

```cpp
constexpr OptionGroup WIFI_GROUP{"wifi", "Network Settings"};
Config<String> wifiSsid( WIFI_GROUP.opt<String>("ssid", String("MyWiFi"), "WiFi SSID") );
Config<String> wifiPassword( WIFI_GROUP.opt<String>("password", String("secret"), "WiFi Password", true, true) );
```

Template signature (conceptual):

```
template<class T>
ConfigOptions<T> OptionGroup::opt(
   const char* key,
   T defaultValue,
  const char* name,
   bool showInWeb = true,
   bool isPassword = false,
  ConfigCallback callback = nullptr,
   std::function<bool()> showIf = nullptr) const;
```


### Dynamic Visibility (showIf) Pattern

The `showIf` member of `ConfigOptions<T>` is a `std::function<bool()>`. It is evaluated on every JSON generation for the web UI (after each apply/save). Keep it lightweight (simple flag checks) to avoid blocking the loop. Example:


```cpp
Config<bool> enableAdvanced(ConfigOptions<bool>{
  .key = "adv",
  .name = "Enable Advanced",
  .category = "sys",
  .defaultValue = false
});
Config<int> hiddenUnlessEnabled(ConfigOptions<int>{
  .key = "hnum",
  .name = "Hidden Number",
  .category = "sys",
  .defaultValue = 42,
  .showIf = [](){ return enableAdvanced.get(); }
});
```

Two helper factories replace repeating lambdas:

```cpp
showIfTrue(flagConfig);   // visible when flag true
showIfFalse(flagConfig);  // visible when flag false
```

Example:

```cpp
Config<bool> useDhcp( WIFI_GROUP.opt<bool>("dhcp", false, "Use DHCP") );
Config<String> staticIp( WIFI_GROUP.opt<String>("sIP", String("192.168.2.50"), "Static IP", true, false, nullptr, showIfFalse(useDhcp) ) );
```

To replace repeating lambdas like `[this](){ return !this->useDhcp.get(); }`, two helper factories are available:

```cpp
inline std::function<bool()> showIfTrue (const Config<bool>& flag);
inline std::function<bool()> showIfFalse(const Config<bool>& flag);
```

Usage inside a settings struct:

```cpp
struct WiFi_Settings {
  Config<bool> useDhcp;
  Config<String> staticIp;
  Config<String> gateway;
  Config<String> subnet;

  WiFi_Settings() :
    useDhcp(    WIFI_GROUP.opt<bool>("dhcp", false, "Use DHCP") ),
    staticIp(   WIFI_GROUP.opt<String>("sIP",    String("192.168.2.126"), "Static IP",    true, false, nullptr, showIfFalse(useDhcp) ) ),
    gateway(    WIFI_GROUP.opt<String>("GW",     String("192.168.2.250"), "Gateway",      true, false, nullptr, showIfFalse(useDhcp) ) ),
    subnet(     WIFI_GROUP.opt<String>("subnet", String("255.255.255.0"), "Subnet-Mask",  true, false, nullptr, showIfFalse(useDhcp) ) )
  { /* addSetting(...) */ }
};
```

Benefits:

- Less visual noise → easier to scan core semantics
- Lower risk of copy/paste mistakes (e.g., forgetting to invert a condition)
- Consistent semantics across different settings groups

## Password / Secret Fields

Set `.isPassword = true` to mask the value in the UI. The real value is stored and only submitted when modified.

## Truncation & Key Safety

Stored key = `<category>_<key>` truncated to 15 chars to satisfy ESP32 NVS length limits. Provide human readable names via name/categoryPretty.

## Changing Values Programmatically

```cpp
wifiSsid.set("NewName");
wifiSsid.save();   // persist single value
cfg.saveAll();     // or batch
```

## Callbacks

Provide `.callback` or call `myConfig.setCallback([](T v){ ... });` after construction. Callback fires only when value changes.

## Dynamic Visibility Pattern

```cpp
Config<bool> enableAdv( WIFI_GROUP.opt<bool>("adv", false, "Enable Advanced") );
Config<int> hiddenNum( WIFI_GROUP.opt<int>("hnum", 42, "Hidden Number", true, false, nullptr, showIfTrue(enableAdv) ) );
```
