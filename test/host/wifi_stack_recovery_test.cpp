// [MOCKED!] Arduino driver/clock; production reset methods are extracted.
#include <cassert>
#include <cstdint>
#include <iostream>

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;

constexpr int WIFI_STA = 1;
constexpr int WIFI_OFF = 0;
constexpr int WIFI_POWER_19_5dBm = 78;
#define WIFI_LOG(...) ((void)0)
#define WIFI_LOG_VERBOSE(...) ((void)0)

struct wifi_init_config_t {
  int static_tx_buf_num = 8;
  int dynamic_tx_buf_num = 0;
  int tx_buf_type = 0;
  int cache_tx_buf_num = 32;
  int static_rx_buf_num = 8;
  int dynamic_rx_buf_num = 32;
  int unrelated = 12345;
};
#define WIFI_INIT_CONFIG_DEFAULT() \
  wifi_init_config_t {}

uint32_t clockMs = 0;
unsigned long millis() {
  return clockMs;
}
wifi_init_config_t captured;
unsigned initCalls = 0;
unsigned nativeCalls = 0;
esp_err_t initResult = ESP_OK;
esp_err_t esp_wifi_init(const wifi_init_config_t* config) {
  captured = *config;
  ++initCalls;
  ++nativeCalls;
  return initResult;
}
void esp_wifi_stop() {
  ++nativeCalls;
}
void esp_wifi_deinit() {
  ++nativeCalls;
}
static wifi_init_config_t arduinoBootBufferConfig();

struct FakeWiFi {
  bool staticBuffers = false;
  unsigned modeCalls = 0;
  int failedMode = -1;
  bool ota = false;
  bool useStaticBuffers() const {
    return staticBuffers;
  }
  bool mode(int value) {
    assert(value == WIFI_STA || value == WIFI_OFF);
    ++modeCalls;
    if (value == failedMode)
      return false;
    if (value == WIFI_STA)
      captured = arduinoBootBufferConfig();
    return true;
  }
  void disconnect(bool powerOff) {
    assert(!powerOff);
  }
  void setAutoReconnect(bool) {}
  void persistent(bool) {}
  void setTxPower(int) {}
} WiFi;
struct WiFiGenericClass {
  static bool useStaticBuffers() {
    return WiFi.useStaticBuffers();
  }
};
#include "arduino-boot-buffer-policy.inc"

struct ConfigManagerWiFi {
  bool stackResetInProgress = true;
  bool connectAfterStackReset = true;
  uint8_t stackResetStep = 2;
  uint32_t stackResetStepAtMs = 0;
  unsigned policyCalls = 0;
  bool bleCoexistenceReady_ = true;
  bool isOtaActive_() const {
    return WiFi.ota;
  }
  void applyPowerSave_() {
    ++policyCalls;
  }
  bool advanceStackReset_();
  void startStackReset_();
};
#include "wifi-stack-recovery.inc"

static void runSuccess(ConfigManagerWiFi& reset) {
  clockMs += 500;
  assert(!reset.advanceStackReset_());
  clockMs += 200;
  assert(!reset.advanceStackReset_());
  clockMs += 200;
  assert(!reset.advanceStackReset_());
  clockMs += 100;
  assert(reset.advanceStackReset_());
  assert(!reset.stackResetInProgress && reset.policyCalls == 1);
}

static void checkPolicy(bool staticBuffers) {
  WiFi.staticBuffers = staticBuffers;
  WiFi.failedMode = -1;
  WiFi.modeCalls = nativeCalls = 0;
  clockMs = 10;
  ConfigManagerWiFi reset;
  reset.stackResetInProgress = false;
  reset.startStackReset_();
  assert(reset.stackResetInProgress && !reset.bleCoexistenceReady_);
  assert(nativeCalls == 0); // Arduino owns the complete driver lifecycle.
  runSuccess(reset);
  assert(nativeCalls == 0 && WiFi.modeCalls == 2);
  const auto boot = arduinoBootBufferConfig();
  assert(captured.static_tx_buf_num == boot.static_tx_buf_num);
  assert(captured.dynamic_tx_buf_num == boot.dynamic_tx_buf_num);
  assert(captured.tx_buf_type == boot.tx_buf_type);
  assert(captured.cache_tx_buf_num == boot.cache_tx_buf_num);
  assert(captured.static_rx_buf_num == boot.static_rx_buf_num);
  assert(captured.dynamic_rx_buf_num == boot.dynamic_rx_buf_num);
  assert(captured.unrelated == 12345 && boot.unrelated == 12345);
}

static void checkFailure(int failedMode) {
  WiFi.failedMode = failedMode;
  WiFi.modeCalls = nativeCalls = 0;
  clockMs = 1000;
  ConfigManagerWiFi failed;
  failed.stackResetInProgress = false;
  failed.stackResetStep = 0;
  failed.startStackReset_();
  if (failedMode == WIFI_STA) {
    clockMs += 500;
    assert(!failed.advanceStackReset_());
    clockMs += 200;
    assert(!failed.advanceStackReset_());
    clockMs += 200;
    assert(!failed.advanceStackReset_());
  }
  assert(!failed.stackResetInProgress && failed.stackResetStep == 0);
  assert(!failed.connectAfterStackReset && failed.stackResetStepAtMs == 0);
  const auto attempts = WiFi.modeCalls;
  clockMs += 10000;
  assert(!failed.advanceStackReset_());
  assert(WiFi.modeCalls == attempts && failed.policyCalls == 0);
  WiFi.failedMode = -1;
  failed.startStackReset_();
  assert(failed.stackResetInProgress);
  runSuccess(failed);
}

int main(int argc, char**) {
  if (argc == 1) {
    checkPolicy(false);
    checkPolicy(true);
  }
  checkFailure(WIFI_OFF);
  checkFailure(WIFI_STA);
  WiFi.ota = true;
  WiFi.modeCalls = 0;
  ConfigManagerWiFi guarded;
  guarded.stackResetInProgress = false;
  guarded.startStackReset_();
  assert(!guarded.stackResetInProgress && WiFi.modeCalls == 0);
  guarded.stackResetInProgress = true;
  assert(!guarded.advanceStackReset_() && WiFi.modeCalls == 0);
  std::cout << "[MOCKED!] Arduino-owned recovery preserves buffer choice; mode failures never complete; OTA guarded\n";
}
