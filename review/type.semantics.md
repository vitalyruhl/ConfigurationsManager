# Type & value-semantics audit

## 1) auto / decltype(auto) findings

| Location | Code | Issue | Safer alternative | Notes |
|---|---|---|---|---|
| `src/ConfigManager.h` | `auto callbacks = msgIt->second;` | Copies callback map before lookup/dispatch. | `const auto& callbacks = msgIt->second;` | Safe reference binding; avoids map copy in hot path. |
| `src/ConfigManager.h` | `auto it = ioPinRoles.find(key);` | No issue. iterator copy is intended and cheap. | Keep as-is. | Included for audit completeness. |
| `src/logging/LoggingManager.cpp` | `const auto tsMode = getTimestampMode();` | No issue. value copy is intended enum copy. | Keep as-is. | No behavior risk. |

## 2) string / string_view findings

| Symbol/Location | Current signature/usage | Issue | Proposed change | Lifetime notes |
|---|---|---|---|---|
| `ConfigManagerWiFi::setAccessPointMacFilter` (`src/wifi/WiFiManager.h`) | `void setAccessPointMacFilter(const String& macAddress)` | Arduino `String` API surface; switching to `std::string_view` would add conversion friction and mixed string models. | Keep as-is. | Internal storage uses `String`; lifetime is owned after assignment. |
| `ConfigManagerWiFi::setAccessPointMacPriority` (`src/wifi/WiFiManager.h`) | `void setAccessPointMacPriority(const String& macAddress)` | Same as above. | Keep as-is. | Same lifetime model. |
| Project-wide public API | Predominantly `String` and `const char*` | No clearly safe mechanical migration to `std::string_view` in embedded Arduino-facing API. | Defer broad migration. | Would require coordinated ABI/API design pass. |

## Applied in phase 2.6
- Applied safe change: `auto callbacks = msgIt->second;` → `const auto& callbacks = msgIt->second;` in `src/ConfigManager.h`.
- No additional safe `string_view` refactors applied.
