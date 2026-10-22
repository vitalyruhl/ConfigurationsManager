#include "wifi/ConnectivityCheck.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
using String = std::string;
static unsigned long clockNow = 0;
static unsigned long millis() {
  return clockNow;
}
static void delay(unsigned long) {}
static void markRestartCause(uint32_t) {}
static void logMessage(const char*, ...) {}
constexpr uint32_t kRestartCauseWiFiAutoReboot = 1;
constexpr unsigned long kTransientGraceWindowMs = 10000, kTransientLogIntervalMs = 5000;
#define WIFI_LOG(...) logMessage(__VA_ARGS__)
#define WIFI_LOG_VERBOSE(...) logMessage(__VA_ARGS__)
enum { WL_IDLE_STATUS,
       WL_NO_SSID_AVAIL,
       WL_SCAN_COMPLETED,
       WL_CONNECTED,
       WL_CONNECTION_LOST,
       WL_DISCONNECTED,
       WIFI_AP = 10,
       WIFI_STA };
enum WiFiManagerState { WIFI_STATE_DISCONNECTED,
                        WIFI_STATE_CONNECTING,
                        WIFI_STATE_CONNECTED,
                        WIFI_STATE_AP_MODE,
                        WIFI_STATE_RECONNECTING };
// [MOCKED!] Platform state, socket admission and physical reset only; actual manager methods are extracted unchanged.
struct IPAddress {
  uint8_t bytes[4]{};
  uint8_t operator[](unsigned i) const {
    return bytes[i];
  }
  String toString() const {
    return "192.168.2.126";
  }
};
struct Wifi {
  int statusCode = WL_CONNECTED, mode = WIFI_STA;
  IPAddress ip{{192, 168, 2, 126}}, route{{192, 168, 2, 1}};
  int status() const {
    return statusCode;
  }
  int getMode() const {
    return mode;
  }
  IPAddress localIP() const {
    return ip;
  }
  IPAddress gatewayIP() const {
    return route;
  }
  int RSSI() const {
    return -50;
  }
} WiFi;
static bool isValidStationIp(const IPAddress& ip) {
  return ip[0] || ip[1] || ip[2] || ip[3];
}
struct Esp {
  int resets = 0;
  void restart() {
    ++resets;
  }
} ESP;
struct Wire : cm::detail::ConnectivityTransport {
  std::vector<uint32_t> targets;
  bool ready = false;
  bool start(uint32_t target, uint16_t, uint32_t) override {
    targets.push_back(target);
    return true;
  }
  bool poll() override {
    bool r = ready;
    ready = false;
    return r;
  }
  void cancel() override {
    ready = false;
  }
};
struct ConfigManagerWiFi {
  bool autoRebootEnabled = true, ota = false, initialized = true, connectAfterStackReset = false, transientStatusActive = false, roamingReconnectPending = false;
  int transientStatusCode = WL_IDLE_STATUS;
  String roamingTargetBSSID;
  unsigned long autoRebootTimeoutMs = 60000, lastGoodConnectionMillis = 77, connectionMonitoringStartedMillis = 0;
  unsigned long transientStatusStartedMillis = 0, transientStatusLastLogMillis = 0;
  WiFiManagerState currentState = WIFI_STATE_CONNECTED;
  cm::detail::ConnectivityCheck connectivityCheck_;
  Wire connectivitySocket_;
  bool isOtaActive_() const {
    return ota;
  }
  bool advanceStackReset_() {
    return false;
  }
  void beginWiFiConnection_() {}
  void processPendingRoamingReconnect_() {}
  void attemptConnect() {}
  void handleReconnection() {}
  void checkSmartRoaming() {}
  void logNoSsidAvailScan_() {}
  void transitionToState(WiFiManagerState state) {
    currentState = state;
  }
  String getStatusString() const {
    return "connected";
  }
  String getWiFiStatusString(int) const {
    return "connected";
  }
  void updateConnectivity_(uint32_t, bool, bool);
  bool setConnectivityCheckTarget(const String&);
  void checkAutoReboot();
  void update();
  void pauseConnectivityForSynchronousOta();
};
#include "connectivity-manager.inc"
int main() {
  ConfigManagerWiFi manager;
  clockNow = 100;
  assert(manager.setConnectivityCheckTarget("gateway"));
  manager.update();
  assert(manager.lastGoodConnectionMillis == 77 && manager.connectivitySocket_.targets.back() == 0xc0a80201u);
  manager.connectivitySocket_.ready = true;
  clockNow = 200;
  manager.update();
  assert(manager.lastGoodConnectionMillis == 200);
  clockNow = 201;
  assert(manager.setConnectivityCheckTarget("gateway")); // Reapplying same setting must not cancel or reset proof.
  WiFi.route = {{192, 168, 3, 1}};
  manager.connectivitySocket_.ready = true;
  clockNow = 300;
  manager.update();
  assert(manager.lastGoodConnectionMillis == 200 && manager.connectivitySocket_.targets.back() == 0xc0a80301u);
  assert(!manager.setConnectivityCheckTarget("192.168.3.999"));
  manager.ota = true;
  clockNow = 1000000;
  manager.update();
  assert(ESP.resets == 0);
  manager.ota = false;
  clockNow = 1000001;
  manager.update();
  assert(ESP.resets == 0);
  clockNow = 1060000;
  manager.update();
  assert(ESP.resets == 0);
  clockNow = 1060001;
  manager.update();
  assert(ESP.resets == 1);
  ESP.resets = 0;
  WiFi.mode = WIFI_AP;
  clockNow = 2000000;
  manager.update();
  assert(ESP.resets == 0);
  WiFi.mode = WIFI_STA;
  clockNow = 2000001;
  manager.update();
  assert(ESP.resets == 0);
  clockNow = 2060001;
  manager.update();
  assert(ESP.resets == 1);
  ESP.resets = 0;
  clockNow = 3000000;
  assert(manager.setConnectivityCheckTarget("off"));
  manager.update();
  assert(manager.lastGoodConnectionMillis == 3000000 && ESP.resets == 0);
  WiFi.ip = {};
  clockNow = 3060000;
  manager.update();
  assert(manager.lastGoodConnectionMillis == 3000000 && ESP.resets == 1);
  // ArduinoOTA handles a transfer synchronously: no WiFi update can observe its active flag.
  ESP.resets = 0;
  WiFi.ip = {{192, 168, 2, 126}};
  WiFi.mode = WIFI_STA;
  clockNow = 0;
  ConfigManagerWiFi synchronous;
  assert(synchronous.setConnectivityCheckTarget("gateway"));
  synchronous.update();
  synchronous.ota = true;
  synchronous.pauseConnectivityForSynchronousOta();
  clockNow = 70000;
  synchronous.ota = false;
  synchronous.update(); // Transfer aborted after longer than WiFiRb.
  assert(ESP.resets == 0);
  clockNow = 129999;
  synchronous.update();
  assert(ESP.resets == 0);
  clockNow = 130000;
  synchronous.update();
  assert(ESP.resets == 1);
  std::puts("Connectivity actual WiFiManager update: target proof, dynamic DHCP gateway, OTA/AP grace, off and missing-IP expiry passed");
}
