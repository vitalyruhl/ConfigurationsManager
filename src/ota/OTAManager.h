#pragma once

#include <ArduinoOTA.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>
#include <functional>
#include "../ConfigManagerConfig.h"

// Forward declaration
class ConfigManagerClass;

#if CM_ENABLE_OTA
struct OtaUploadContext {
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
    bool otaEnabled;
    bool otaInitialized;
    String otaPassword;
    String otaHostname;
    ConfigManagerClass* configManager;

    // Callbacks
    RebootCallback rebootCallback;
    LogCallback logCallback;

    // Helper methods
    void log(const char* format, ...) const;
    void cleanup(AsyncWebServerRequest* request = nullptr);

public:
    ConfigManagerOTA();
    ~ConfigManagerOTA();

    // Initialization
    void begin(ConfigManagerClass* cm);
    void setCallbacks(RebootCallback reboot, LogCallback logger);

#if CM_ENABLE_OTA
    // OTA setup and management
    void setup(const String& hostname, const String& password = "");
    void enable(bool enabled = true);
    void disable() { enable(false); }
    void setPassword(const String& password);
    void setHostname(const String& hostname);

    // Runtime handling
    void handle();
    bool isEnabled() const { return otaEnabled; }
    bool isInitialized() const { return otaInitialized; }
    bool isActive() const;

    // Web routes for HTTP OTA
    void setupWebRoutes(AsyncWebServer* server);
    void handleOTAUpload(AsyncWebServerRequest* request);
    void handleOTAUploadData(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final);

    // Status and info
    String getStatus() const;
    String getHostname() const { return otaHostname; }
    bool hasPassword() const { return !otaPassword.isEmpty(); }

#else
    // Disabled stubs
    void setup(const String&, const String& = "") {}
    void enable(bool = true) {}
    void disable() {}
    void setPassword(const String&) {}
    void setHostname(const String&) {}
    void handle() {}
    bool isEnabled() const { return false; }
    bool isInitialized() const { return false; }
    bool isActive() const { return false; }
    void setupWebRoutes(AsyncWebServer*) {}
    String getStatus() const { return "disabled"; }
    String getHostname() const { return ""; }
    bool hasPassword() const { return false; }
#endif
};