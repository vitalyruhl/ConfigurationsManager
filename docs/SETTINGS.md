# Settings & Configuration API

This document describes how to declare, register, persist and display configuration values using ConfigurationsManager.

## Core Concepts

- Config<T>: Lightweight wrapper around a persisted value (Preferences / NVS).
- ConfigOptions<T>: Aggregate initialization structure used to construct a Config<T>.
- Registration: Add each Config<T> to the manager via addSetting().
- Persistence: loadAll() on startup, saveAll() (or individual save) after changes.
- Visibility: showIf predicate determines whether a setting appears in the web UI JSON.

## Declaring a Setting

```cpp
Config<int> sample(ConfigOptions<int>{
  .keyName = "interval",
  .category = "main",
  .defaultValue = 30,
  .prettyName = "Update Interval (s)",
  .prettyCat = "Main Settings",
  .showInWeb = true,
  .cb = [](){ Serial.println("Interval changed"); }
});
```

Add to manager and load persisted value:

```cpp
cfg.addSetting(&sample);
cfg.loadAll();
```

## OptionGroup (Boilerplate Reduction)

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
   const char* prettyName,
   bool showInWeb = true,
   bool isPassword = false,
   ConfigCallback cb = nullptr,
   std::function<bool()> showIf = nullptr) const;
```

## Visibility Helpers

Two helper factories replace repeating lambdas:

```cpp
showIfTrue(flagConfig);   // visible when flag true
showIfFalse(flagConfig);  // visible when flag false
```

Example:

```cpp
Config<bool> useDhcp( WIFI_GROUP.opt<bool>("dhcp", false, "Use DHCP") );
Config<String> staticIp( WIFI_GROUP.opt<String>("sIP", String("192.168.2.50"), "Static IP", true, false, nullptr, showIfFalse(useDhcp) );
```

## Password / Secret Fields

Set `.isPassword = true` to mask the value in the UI. The real value is stored and only submitted when modified.

## Truncation & Key Safety

Stored key = `<category>_<keyName>` truncated to 15 chars to satisfy ESP32 NVS length limits. Provide human readable names via prettyName/prettyCat.

## Changing Values Programmatically

```cpp
wifiSsid.set("NewName");
wifiSsid.save();   // persist single value
cfg.saveAll();     // or batch
```

## Callbacks

Provide `.cb` or call `myConfig.setCallback([](){ ... });` after construction. Callback fires only when value changes.

## Dynamic Visibility Pattern

```cpp
Config<bool> enableAdv( WIFI_GROUP.opt<bool>("adv", false, "Enable Advanced") );
Config<int> hiddenNum( WIFI_GROUP.opt<int>("hnum", 42, "Hidden Number", true, false, nullptr, showIfTrue(enableAdv) );
```

## Migration Tips

1. Introduce one OptionGroup per logical category.
2. Refactor gradually—legacy ConfigOptions initialization can coexist.
3. Prefer showIfTrue/showIfFalse for simple toggles.
4. Keep showIf predicates fast (no I/O, minimal logic).
5. Use callbacks for side-effects; avoid lengthy operations inside them.

## Planned Enhancements

- Category ordering number
- Per-setting description/tooltip text
- Reset-to-default per setting
- i18n support

