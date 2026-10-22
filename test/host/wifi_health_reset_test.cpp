#include "wifi/detail/WiFiHealthConfig.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
namespace wifi_health_reset_fixture {
#define CM_ENABLE_WIFI 1
#define CM_CORE_LOG(...) ((void)0)
// Platform storage/OTA are mocked; production reset, load and queue methods are extracted unchanged.
struct String : std::string {
  using std::string::string;
  void toLowerCase() {
    std::transform(begin(), end(), begin(), [](unsigned char c) { return std::tolower(c); });
  }
  int indexOf(const String& text) const {
    const auto p = find(text);
    return p == npos ? -1 : static_cast<int>(p);
  }
};
enum class SettingType { INT,
                         STRING };
struct Preferences {
  bool available = true;
  bool begin(const char*, bool) {
    return available;
  }
  void end() {}
};
struct BaseSetting {
  virtual ~BaseSetting() = default;
  virtual void setDefault() = 0;
  virtual void load(Preferences&) = 0;
  virtual void save() = 0;
  virtual SettingType getType() const = 0;
  virtual const char* getKeyHint() const = 0;
  bool shouldPersist() const {
    return true;
  }
  const char* getCategory() const {
    return "WiFi";
  }
  const char* getDisplayName() const {
    return getKeyHint();
  }
};
template <class T>
struct Config : BaseSetting {
  const char* key;
  T value, defaultValue, saved;
  Config(const char* hint, T initial, T fallback)
      : key(hint), value(initial), defaultValue(fallback), saved(initial) {}
  void setDefault() override {
    value = defaultValue;
  }
  void load(Preferences&) override {
    value = saved;
  }
  void save() override {
    saved = value;
  }
  SettingType getType() const override {
    return std::is_same<T, int>::value ? SettingType::INT : SettingType::STRING;
  }
  const char* getKeyHint() const override {
    return key;
  }
  const T& get() const {
    return value;
  }
};
struct Harness {
  std::vector<BaseSetting*> settings;
  Preferences prefs;
  struct {
    void setPassword(const String&) {}
    // Retain the OTA integration slot referenced by the extracted reset callback.
    // cppcheck-suppress unusedStructMember
  } otaManager [[maybe_unused]];
  cm::detail::WiFiHealthConfigMailbox mailbox;
  unsigned saves = 0;
  BaseSetting* findSettingByKeyHint(const char*, const char* hint) {
    const auto setting = std::find_if(settings.begin(), settings.end(), [&](const auto* value) { return std::strcmp(value->getKeyHint(), hint) == 0; });
    return setting == settings.end() ? nullptr : *setting;
  }
  void setWiFiAutoRebootTimeout(uint32_t minutes) {
    assert(mailbox.setTimeoutMinutes(minutes));
  }
  void setConnectivityCheckTarget(const String& target) {
    assert(mailbox.setTarget(target.c_str()));
  }
  void saveAll() {
    ++saves;
    for (auto* setting : settings)
      setting->save();
  }
#include "wifi-health-reset.inc"
};
} // namespace wifi_health_reset_fixture

int main() {
  using namespace wifi_health_reset_fixture;
  Config<int> timeout("WiFiRb", 5, 1);
  Config<String> target("WiFiProbe", String("gateway"), String("off"));
  Harness manager;
  manager.settings = {&timeout, &target};
  // A queued Apply must be replaced by the reset values, never resurrected on the next loop.
  manager.setWiFiAutoRebootTimeout(1);
  manager.setConnectivityCheckTarget("192.0.2.77");
  manager.resetDefaults();
  assert(manager.saves == 1 && timeout.saved == 1 && target.saved == "off");
  cm::detail::WiFiHealthConfig update;
  assert(manager.mailbox.take(update));
  assert(update.targetChanged && update.timeoutChanged);
  assert(update.timeoutMinutes == 1 && std::strcmp(update.target.data(), "off") == 0);
  assert(!manager.mailbox.take(update));
  timeout.saved = 120;
  target.saved = "gateway";
  manager.loadAll();
  assert(timeout.value == 120 && target.value == "gateway");
  assert(manager.mailbox.take(update) && update.timeoutChanged && update.targetChanged);
  assert(update.timeoutMinutes == 120 && std::strcmp(update.target.data(), "gateway") == 0);
  manager.prefs.available = false;
  bool failed = false;
  try {
    manager.loadAll();
  } catch (const std::runtime_error&) {
    failed = true;
  }
  assert(failed && !manager.mailbox.take(update));
  // Custom sketches may omit either or both core WiFi settings.
  Harness empty;
  empty.resetDefaults();
  empty.loadAll();
  empty.queueWiFiHealthSetting(nullptr);
  assert(!empty.mailbox.take(update));
  Harness partial;
  partial.settings = {&target};
  partial.resetDefaults();
  assert(partial.mailbox.take(update) && update.targetChanged && !update.timeoutChanged);
  assert(std::strcmp(update.target.data(), "off") == 0);
  std::cout << "Extracted reset/load WiFi health synchronization checks passed\n";
}
