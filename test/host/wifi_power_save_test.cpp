// Hardware and NVS are mocked; production policy, load, queue and driver methods are extracted unchanged.
#include "ConfigManagerConfig.h"
#include "wifi/detail/WiFiHealthConfig.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <mutex>
#include <atomic>
#include <string>
#include <type_traits>
#include <ArduinoJson.h>
using String = std::string;
using esp_err_t = int;
enum wifi_ps_type_t { WIFI_PS_NONE,
                      WIFI_PS_MIN_MODEM,
                      WIFI_PS_MAX_MODEM };
constexpr int ESP_OK = 0;
constexpr int ESP_ERR_INVALID_STATE = 259;
constexpr int WIFI_MODE_STA = 1;
constexpr int WIFI_MODE_NULL = 0;
constexpr int ESP_ERR_WIFI_NOT_INIT = 12289;
struct FakeWiFi {
  int mode = 0, calls = 0;
  bool success = true;
  wifi_ps_type_t effective = WIFI_PS_MIN_MODEM;
  wifi_ps_type_t cached = WIFI_PS_MIN_MODEM;
  int getMode() const {
    return mode;
  }
  bool setSleep(wifi_ps_type_t next) {
    ++calls;
    if (cached == next)
      return false; // Arduino 2.x returns false without touching the native driver.
    cached = next;
    if (success)
      effective = next;
    return success;
  }
} WiFi;
uint32_t fakeMillis = 0;
uint32_t millis() {
  return fakeMillis;
}
int readError = 0;
int nativeSetError = 0;
unsigned nativeSetCalls = 0;
esp_err_t esp_wifi_set_ps(wifi_ps_type_t mode) {
  ++nativeSetCalls;
  if (nativeSetError)
    return nativeSetError;
  if (!WiFi.success)
    return 124;
  WiFi.effective = mode;
  return ESP_OK;
}
esp_err_t esp_wifi_get_ps(wifi_ps_type_t* mode) {
  if (!readError)
    *mode = WiFi.effective;
  return readError;
}
#include "wifi-power-diagnostics.inc"
class ConfigManagerWiFi {
public:
  bool stackResetInProgress = false;
  mutable std::mutex powerSaveMutex_;
  WiFiPowerSaveDiagnostics powerSaveDiagnostics_;
  std::atomic<bool> bleCoexistenceReady_{false};
  uint32_t bleCoexistenceAttemptMs_ = 0;
  static void setConnectivityCheckTarget(const String&) {}
  static void setAutoRebootTimeout(uint32_t) {}
  void setPowerSave(bool enabled);
  void requireBleCoexistence();
  void retryBleCoexistence();
  bool isBleCoexistenceReady() const {
    return bleCoexistenceReady_.load();
  }
  void applyPowerSave_();
  WiFiPowerSaveDiagnostics getPowerSaveDiagnostics() const;
};
#include "wifi-power-driver.inc"
struct Preferences {
  bool exists = false, stored = false;
  unsigned writes = 0;
  bool isKey(const char*) const {
    return exists;
  }
  bool getBool(const char*, bool fallback) const {
    return exists ? stored : fallback;
  }
  size_t putBool(const char*, bool value) {
    exists = true;
    stored = value;
    ++writes;
    return 1;
  }
  static String getString(const char*, const String& value) {
    return value;
  }
  static int getInt(const char*, int value) {
    return value;
  }
  static float getFloat(const char*, float value) {
    return value;
  }
  static size_t putString(const char*, const String&) {
    return 1;
  }
  static size_t putInt(const char*, int) {
    return 1;
  }
  static size_t putFloat(const char*, float) {
    return 1;
  }
};
template <class T>
struct ConfigOptions {
  const char* key = nullptr;
  const char* name = nullptr;
  const char* category = nullptr;
  T defaultValue{};
  // Match the production aggregate fields, although the stub only serializes values.
  // cppcheck-suppress unusedStructMember
  [[maybe_unused]] bool showInWeb = true;
  // cppcheck-suppress unusedStructMember
  [[maybe_unused]] int sortOrder = 100;
};
enum class SettingType { BOOL,
                         STRING };
struct BaseSetting {
  virtual ~BaseSetting() = default;
  virtual void load(Preferences&) = 0;
  virtual SettingType getType() const = 0;
  virtual bool fromJSON(const JsonVariant&) = 0;
  virtual void toJSON(JsonObject&) const = 0;
};
template <class T>
struct Config : BaseSetting {
  ConfigOptions<T> options;
  T value, defaultValue;
  bool modified = false, isPassword = false;
  explicit Config(ConfigOptions<T> o)
      : options(o), value(o.defaultValue), defaultValue(o.defaultValue) {}
  SettingType getType() const override {
    return std::is_same_v<T, bool> ? SettingType::BOOL : SettingType::STRING;
  }
  static bool hasError() {
    return false;
  }
  static const char* getError() {
    return "";
  }
  const char* getDisplayName() const {
    return options.name;
  }
  const char* getCategory() const {
    return options.category;
  }
  const char* getKey() const {
    return options.key;
  }
  const T& get() const {
    return value;
  }
  void set(T next) {
    value = next;
  }
  template <class... A>
  static void log(A...) {}
  template <class... A>
  static void logVerbose(A...) {}
  void toJSON(JsonObject& obj) const override {
    obj[getKey()]["value"] = value;
  }
#include "wifi-power-config.inc"
};
#include "wifi-power-setting.inc"
struct ConfigManagerClass {
  cm::detail::WiFiHealthConfigMailbox wifiHealthConfig;
  ConfigManagerWiFi wifiManager;
  BaseSetting* sleepSetting = nullptr;
  BaseSetting* findSettingByKeyHint(const char*, const char* key) {
    return std::string(key) == "WiFiPowerSave" ? sleepSetting : nullptr;
  }
#include "wifi-power-api.inc"
};
int main() {
  static_assert(CM_WIFI_POWER_SAVE_DEFAULT == EXPECTED_DEFAULT);
  ConfigManagerWiFi driver;
  auto d = driver.getPowerSaveDiagnostics();
  assert(d.requested == bool(EXPECTED_DEFAULT) && !d.applyAttempted);
  driver.setPowerSave(!bool(EXPECTED_DEFAULT));
  assert(WiFi.calls == 0); // A pre-start request must not start or touch the radio.
  WiFi.mode = WIFI_MODE_STA;
  driver.applyPowerSave_();
  d = driver.getPowerSaveDiagnostics();
  assert(d.applyAttempted && d.applySucceeded && d.readError == ESP_OK);
  assert(d.effectiveMode == (!bool(EXPECTED_DEFAULT) ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE));
  // The same production helper is invoked by initial connect, stack reset and retry.
  for (int path = 0; path < 3; ++path) {
    WiFi.effective = WIFI_PS_MAX_MODEM;
    driver.applyPowerSave_();
    assert(driver.getPowerSaveDiagnostics().effectiveMode == d.effectiveMode);
  }
  driver.stackResetInProgress = true;
  int calls = WiFi.calls;
  driver.setPowerSave(false);
  assert(WiFi.calls == calls);
  driver.stackResetInProgress = false;
  driver.applyPowerSave_();
  assert(driver.getPowerSaveDiagnostics().effectiveMode == WIFI_PS_NONE);
  // Native deinit/init resets PS to MIN while Arduino retains its NONE cache.
  assert(WiFi.cached == WIFI_PS_NONE);
  WiFi.effective = WIFI_PS_MIN_MODEM;
  driver.applyPowerSave_();
  assert(driver.getPowerSaveDiagnostics().effectiveMode == WIFI_PS_NONE);
  driver.applyPowerSave_();
  assert(driver.getPowerSaveDiagnostics().applySucceeded);
  WiFi.success = false;
  driver.setPowerSave(true);
  d = driver.getPowerSaveDiagnostics();
  assert(d.requested && !d.applySucceeded && d.applyError == 124 && d.effectiveMode == WIFI_PS_NONE);
  readError = 123;
  d = driver.getPowerSaveDiagnostics();
  assert(d.readError == 123 && d.effectiveMode == -1); // Never infer ON/OFF after failed readback.
  readError = 0;
  ConfigManagerClass owner;
  owner.setWiFiPowerSave(true);
  owner.setWiFiPowerSave(false);
  cm::detail::WiFiHealthConfig update;
  assert(owner.wifiHealthConfig.take(update) && update.powerSaveChanged && !update.powerSave);
  assert(!owner.wifiHealthConfig.take(update));
  owner.setWiFiPowerSave(false);
  owner.setWiFiPowerSave(true);
  assert(owner.wifiHealthConfig.take(update) && update.powerSave);
  cm::WiFiPowerSaveSetting setting;
  Preferences prefs;
  setting.load(prefs);
  assert(setting.get() == bool(EXPECTED_DEFAULT) && prefs.writes == 1);
  prefs.stored = !bool(EXPECTED_DEFAULT);
  setting.load(prefs);
  assert(setting.get() == !bool(EXPECTED_DEFAULT) && prefs.writes == 1);
  owner.setWiFiPowerSave(bool(EXPECTED_DEFAULT));
  assert(prefs.stored == !bool(EXPECTED_DEFAULT) && prefs.writes == 1);
  // Boot uses the loaded setting; later explicit API requests supersede the queued loaded value.
  WiFi.mode = 0;
  owner.sleepSetting = &setting;
  owner.setWiFiPowerSave(setting.get());
  owner.initializeWiFiHealthConfig();
  assert(owner.wifiManager.getPowerSaveDiagnostics().requested == setting.get());
  owner.setWiFiPowerSave(setting.get());
  owner.setWiFiPowerSave(!setting.get());
  owner.initializeWiFiHealthConfig();
  assert(owner.wifiManager.getPowerSaveDiagnostics().requested == !setting.get());
  WiFi.success = true;
  WiFi.mode = WIFI_MODE_STA;
  owner.wifiManager.applyPowerSave_();
  assert(owner.wifiManager.getPowerSaveDiagnostics().effectiveMode == (!setting.get() ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE));
  // No registered bundle: build default applies, without creating a setting or touching NVS.
  ConfigManagerClass minimal;
  minimal.initializeWiFiHealthConfig();
  assert(minimal.wifiManager.getPowerSaveDiagnostics().requested == bool(EXPECTED_DEFAULT));
  JsonDocument doc;
  for (const char* invalid : {"1", "0", "\"false\"", "null", "[]", "{}"}) {
    deserializeJson(doc, invalid);
    assert(!setting.fromJSON(doc.as<JsonVariant>()));
  }
  doc.set(true);
  assert(setting.fromJSON(doc.as<JsonVariant>()) && setting.get());
  setting.diagnostics = [&driver] { return driver.getPowerSaveDiagnostics(); };
  readError = 123;
  doc.clear();
  auto obj = doc.to<JsonObject>();
  setting.toJSON(obj);
  assert(obj["WiFiPowerSave"]["powerSave"]["effectiveMode"] == -1);
  assert(obj["WiFiPowerSave"]["powerSave"]["readError"] == 123);
  // An earlier successful MIN is not an acknowledgment of a new BLE lease.
  ConfigManagerClass coex;
  WiFi.mode = WIFI_MODE_STA;
  WiFi.success = true;
  readError = 0;
  coex.wifiManager.setPowerSave(true);
  assert(coex.wifiManager.getPowerSaveDiagnostics().effectiveMode == WIFI_PS_MIN_MODEM);
  assert(!coex.isWiFiBleCoexistenceReady());
  coex.requestWiFiBleCoexistence();
  coex.setWiFiPowerSave(false); // Same batch must pin coexistence before applying NONE.
  assert(!coex.isWiFiBleCoexistenceReady());
  const unsigned beforeLease = nativeSetCalls;
  coex.applyWiFiHealthConfig();
  assert(nativeSetCalls > beforeLease && coex.isWiFiBleCoexistenceReady());
  auto coexDiag = coex.wifiManager.getPowerSaveDiagnostics();
  assert(!coexDiag.requested && coexDiag.bleCoexistenceRequired);
  assert(coexDiag.applySucceeded && coexDiag.effectiveMode == WIFI_PS_MIN_MODEM);
  // Callback changes, settings reset and native WiFi reset never clear the lease.
  for (bool requested : {true, false, false}) {
    coex.setWiFiPowerSave(requested);
    coex.applyWiFiHealthConfig();
    assert(coex.wifiManager.getPowerSaveDiagnostics().effectiveMode == WIFI_PS_MIN_MODEM);
  }
  WiFi.effective = WIFI_PS_NONE;
  coex.wifiManager.applyPowerSave_();
  assert(coex.wifiManager.getPowerSaveDiagnostics().effectiveMode == WIFI_PS_MIN_MODEM);
  // A new lease must not reuse a prior result after a failed native apply/read.
  for (bool failedRead : {false, true}) {
    nativeSetError = failedRead ? 0 : 124;
    readError = failedRead ? 123 : 0;
    coex.wifiManager.requireBleCoexistence();
    assert(!coex.isWiFiBleCoexistenceReady());
  }
  nativeSetError = 0;
  readError = 0;
  coex.wifiManager.requireBleCoexistence();
  assert(coex.isWiFiBleCoexistenceReady());
  // Failed first-start native Set or Get recovers without reconnect, at bounded cadence.
  for (bool failedRead : {false, true}) {
    ConfigManagerClass recovering;
    fakeMillis = UINT32_MAX - 500u;
    nativeSetError = failedRead ? 0 : 124;
    readError = failedRead ? 123 : 0;
    recovering.requestWiFiBleCoexistence();
    recovering.applyWiFiHealthConfig();
    assert(!recovering.isWiFiBleCoexistenceReady());
    const unsigned attempts = nativeSetCalls;
    nativeSetError = readError = 0;
    fakeMillis += 999u;
    recovering.applyWiFiHealthConfig();
    assert(nativeSetCalls == attempts && !recovering.isWiFiBleCoexistenceReady());
    fakeMillis += 1u;
    recovering.wifiManager.stackResetInProgress = true;
    recovering.applyWiFiHealthConfig();
    assert(nativeSetCalls == attempts);
    recovering.wifiManager.stackResetInProgress = false;
    recovering.applyWiFiHealthConfig();
    assert(nativeSetCalls == attempts + 1 && recovering.isWiFiBleCoexistenceReady());
    fakeMillis += 5000u;
    recovering.applyWiFiHealthConfig();
    assert(nativeSetCalls == attempts + 1); // Acknowledged state is not continually reapplied.
  }
  // BLE-only safe case: both native calls confirm NOT_INIT and WiFi mode is NULL.
  ConfigManagerClass bleOnly;
  nativeSetError = readError = ESP_ERR_WIFI_NOT_INIT;
  WiFi.mode = WIFI_MODE_STA;
  bleOnly.requestWiFiBleCoexistence();
  bleOnly.applyWiFiHealthConfig();
  assert(!bleOnly.isWiFiBleCoexistenceReady());
  WiFi.mode = WIFI_MODE_NULL;
  bleOnly.wifiManager.requireBleCoexistence();
  assert(bleOnly.isWiFiBleCoexistenceReady() && WiFi.cached == WIFI_PS_MIN_MODEM);
  assert(bleOnly.wifiManager.getPowerSaveDiagnostics().effectiveMode == -1);
  nativeSetError = readError = 0;
  WiFi.mode = WIFI_MODE_STA;
  WiFi.effective = WIFI_PS_NONE;
  bleOnly.wifiManager.setPowerSave(false);
  assert(bleOnly.wifiManager.getPowerSaveDiagnostics().effectiveMode == WIFI_PS_MIN_MODEM);
  // Settings-only JSON explains the override without changing the stored request.
  setting.diagnostics = [&coex] { return coex.wifiManager.getPowerSaveDiagnostics(); };
  doc.clear();
  obj = doc.to<JsonObject>();
  setting.toJSON(obj);
  assert(obj["WiFiPowerSave"]["powerSave"]["reason"] == "ble_coexistence");
  coex.setWiFiPowerSave(true);
  coex.applyWiFiHealthConfig();
  doc.clear();
  obj = doc.to<JsonObject>();
  setting.toJSON(obj);
  assert(obj["WiFiPowerSave"]["powerSave"]["reason"] == "requested");
  std::cout << "WiFi power save: default=" << EXPECTED_DEFAULT << ", stored choice, handoff, reapply, driver errors and settings-only JSON passed\n";
}
