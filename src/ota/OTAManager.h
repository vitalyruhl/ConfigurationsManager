#pragma once

#include "../ConfigManagerConfig.h"

#if CM_ENABLE_OTA
#include <ArduinoOTA.h>
#include <Update.h>
#endif

#include <ESPAsyncWebServer.h>
#include <functional>
#include <mutex>
#include <atomic>
#include "detail/OtaLifecycleState.h"
#include "detail/OtaUploadGate.h"

// Forward declaration
class ConfigManagerClass;

#if CM_ENABLE_OTA
struct OtaUploadContext {
  uint32_t sessionId = 0;
  bool hasError = false;
  bool authorized = false;
  bool began = false;
  bool success = false;
  bool probe = false;
  int statusCode = 200;
  String errorReason;
  size_t written = 0;
};
#endif

class ConfigManagerOTA {
public:
  typedef std::function<void()> RebootCallback;
  typedef std::function<void(const char*)> LogCallback;

private:
  static constexpr uint32_t OTA_STALE_TIMEOUT_MS = 120000UL;

  std::atomic<bool> otaEnabled;
  std::atomic<bool> otaInitialized;
  std::atomic<bool> otaActive;
  bool webRoutesConfigured;
  std::atomic<uint32_t> otaLastActivityMs;
  String otaPassword;
  String otaHostname;
  ConfigManagerClass* configManager;

  cm::detail::OtaLifecycleState lifecycle;
  cm::detail::OtaUploadGate uploadGate{lifecycle};
  uint32_t arduinoSessionId = 0;
  std::atomic<uint32_t> pendingAbortSession{0};
  std::atomic<bool> disablePending{false};

  // Callbacks
  RebootCallback rebootCallback;
  LogCallback logCallback;

  // Helper methods
  void log(const char* format, ...) const;
  void cleanup(AsyncWebServerRequest* request = nullptr);
  void markActivity();
  void setActive(bool active);
  void notifyActivityState();
  void checkStaleUpload();
#if CM_ENABLE_OTA
  void sendHttpSuccess(AsyncWebServerRequest* request, OtaUploadContext* context);
#endif

public:
  ConfigManagerOTA();
  ~ConfigManagerOTA();

  // Initialization
  void begin(ConfigManagerClass* cm);
  void setCallbacks(RebootCallback reboot, LogCallback logger);
  // Register before uploads; preserved across setup() and setCallbacks().
  void setLifecycleObserver(std::function<void(const OtaLifecycleEvent&)> observer) {
    lifecycle.setObserver(std::move(observer));
  }

#if CM_ENABLE_OTA
  // OTA setup and management
  void setup(const String& hostname, const String& password = "");
  void enable(bool enabled = true);
  void disable() {
    enable(false);
  }
  void setPassword(const String& password);
  void setHostname(const String& hostname);

  // Runtime handling
  void handle();
  bool isEnabled() const {
    return otaEnabled;
  }
  bool isInitialized() const {
    return otaInitialized;
  }
  bool isActive() const;

  // Web routes for HTTP OTA
  void setupWebRoutes(AsyncWebServer* server);
  void handleOTAUpload(AsyncWebServerRequest* request);
  void handleOTAUploadData(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final);

  // Status and info
  String getStatus() const;
  String getHostname() const {
    return otaHostname;
  }
  bool hasPassword() const {
    return !otaPassword.isEmpty();
  }

#else
  // Disabled stubs
  void setup(const String&, const String& = "") {}
  void enable(bool = true) {}
  void disable() {}
  void setPassword(const String&) {}
  void setHostname(const String&) {}
  void handle() {}
  bool isEnabled() const {
    return false;
  }
  bool isInitialized() const {
    return false;
  }
  bool isActive() const {
    return false;
  }
  void setupWebRoutes(AsyncWebServer*) {}
  String getStatus() const {
    return "disabled";
  }
  String getHostname() const {
    return "";
  }
  bool hasPassword() const {
    return false;
  }
#endif
};
