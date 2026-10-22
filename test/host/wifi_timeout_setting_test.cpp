#include <ArduinoJson.h>
#include <iostream>
#include <string>
#include <type_traits>
using String = std::string;
template<class T> struct ConfigOptions {
  const char* key; const char* name; const char* category;
  T defaultValue; bool showInWeb; int sortOrder;
};
struct JsonSetting { virtual ~JsonSetting() = default; virtual bool fromJSON(const JsonVariant&) = 0; };
// Storage is mocked. The core timeout declaration, validator and Config::fromJSON are extracted unchanged.
template<class T> struct Config : JsonSetting {
  ConfigOptions<T> options; T value;
  explicit Config(ConfigOptions<T> config) : options(config), value(config.defaultValue) {}
  const T& get() const { return value; }
  void set(const T& next) { value = next; }
#include "wifi-timeout-base.inc"
};
#include "wifi-timeout-core.inc"
int main() {
  unsigned failures = 0;
  auto check = [&](bool okay, const char* description) { if (!okay) { ++failures; std::cerr << description << '\n'; } };
  cm::CoreWiFiTimeout settings;
  auto& timeout = settings.rebootTimeoutMin;
  check(timeout.get() == 1, "New default must be one minute");
  check(std::string(timeout.options.key) == "WiFiRb", "Existing storage key must remain unchanged");
  check(std::string(timeout.options.name) == "Reboot if WiFi lost (min)", "Existing public label must remain unchanged");
  JsonDocument doc;
  for (const int minutes : {0, 1, 5, 60, 120, 9999}) {
    doc.set(minutes);
    check(timeout.fromJSON(doc.as<JsonVariant>()) && timeout.get() == minutes, "Valid minute/hour setting must be accepted exactly");
  }
  // A loaded existing value is not replaced by the new fallback.
  timeout.set(120);
  check(timeout.get() == 120, "Existing persisted values must remain intact");
  for (const char* json : {"-1", "10000", "2147483648", "true", "false", "1.5", "1.0", "\"60\"", "null", "[]", "{}"}) {
    const auto result = deserializeJson(doc, json);
    check(!result, "Test input must parse as real JSON");
    check(!timeout.fromJSON(doc.as<JsonVariant>()), "Invalid scalar/type/range must be rejected");
    check(timeout.get() == 120, "Rejected value must leave the current timeout unchanged");
    timeout.set(120);
  }
  if (failures) return 1;
  std::cout << "Extracted core WiFi timeout: one-minute default, integer 0..9999, hour values, preserved key/value checks passed\n";
}
