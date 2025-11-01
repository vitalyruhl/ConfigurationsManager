#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>
#include "../ConfigManagerConfig.h"

#if CM_EMBED_WEBUI
#include "../html_content.h"
#endif

class ConfigManagerClass;

class ConfigManagerWeb {
public:
    // Callback types for integration with ConfigManager
    typedef std::function<String()> JsonProvider;
    typedef std::function<void()> SimpleCallback;
    typedef std::function<bool(const String&, const String&, const String&)> SettingUpdateCallback;
    typedef std::function<void(AsyncWebServerRequest*)> RequestHandler;

private:
    AsyncWebServer* server;
    ConfigManagerClass* configManager;
    bool initialized;

    // Callbacks for ConfigManager integration
    JsonProvider configJsonProvider;
    JsonProvider runtimeJsonProvider;
    JsonProvider runtimeMetaJsonProvider;
    SimpleCallback rebootCallback;
    SimpleCallback resetCallback;
    SettingUpdateCallback settingUpdateCallback;
    SettingUpdateCallback settingApplyCallback;  // For apply-only operations

    // Web content
    bool embedWebUI;
    const char* customHTML;
    size_t customHTMLLen;

    // Settings security
    String settingsPassword;

    // Helper methods
    void setupStaticRoutes();
    void setupAPIRoutes();
    void setupOTARoutes();
    void setupRuntimeRoutes();
    void handleCSSRequest(AsyncWebServerRequest* request);
    void handleJSRequest(AsyncWebServerRequest* request);
    void handleRootRequest(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);

    // Utility methods
    String getContentType(const String& path);
    void sendWebUI(AsyncWebServerRequest* request);
    void enableCORS(AsyncWebServerResponse* response);
    void log(const char* format, ...) const;

public:
    ConfigManagerWeb(AsyncWebServer* webServer = nullptr);
    ~ConfigManagerWeb();

    // Initialization
    void begin(ConfigManagerClass* cm);
    void setCallbacks(
        JsonProvider configJson,
        JsonProvider runtimeJson,
        JsonProvider runtimeMetaJson,
        SimpleCallback reboot,
        SimpleCallback reset,
        SettingUpdateCallback settingUpdate,
        SettingUpdateCallback settingApply
    );

    // Web content management
    void setEmbedWebUI(bool embed);
    void setCustomHTML(const char* html, size_t length);

    // Server control
    AsyncWebServer* getServer() { return server; }
    bool isStarted() const;

    // Route management
    void defineAllRoutes();
    void addCustomRoute(const char* path, WebRequestMethodComposite method, RequestHandler handler);

    // CORS and security
    void enableCORSForAll(bool enable = true);
    void setSettingsPassword(const String& password);

    // Development and debugging
#ifdef development
    void addDevelopmentRoutes();
#endif
};