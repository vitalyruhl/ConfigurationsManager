#include "WebServer.h"
#include "../ConfigManager.h"

// Logging support
#if CM_ENABLE_LOGGING
extern std::function<void(const char*)> ConfigManagerClass_logger;
#define WEB_LOG(...) if(ConfigManagerClass_logger) { char buf[256]; snprintf(buf, sizeof(buf), __VA_ARGS__); ConfigManagerClass_logger(buf); }
#else
#define WEB_LOG(...)
#endif

ConfigManagerWeb::ConfigManagerWeb(AsyncWebServer* webServer) 
    : server(webServer)
    , configManager(nullptr)
    , initialized(false)
    , embedWebUI(true)
    , customHTML(nullptr)
    , customHTMLLen(0)
{
    if (!server) {
        server = new AsyncWebServer(80);
    }
}

ConfigManagerWeb::~ConfigManagerWeb() {
    // Don't delete server here as it might be externally managed
}

void ConfigManagerWeb::begin(ConfigManagerClass* cm) {
    configManager = cm;
    initialized = true;
    WEB_LOG("[Web] Web server module initialized");
}

void ConfigManagerWeb::setCallbacks(
    JsonProvider configJson,
    JsonProvider runtimeJson, 
    JsonProvider runtimeMetaJson,
    SimpleCallback reboot,
    SimpleCallback reset,
    SettingUpdateCallback settingUpdate
) {
    configJsonProvider = configJson;
    runtimeJsonProvider = runtimeJson;
    runtimeMetaJsonProvider = runtimeMetaJson;
    rebootCallback = reboot;
    resetCallback = reset;
    settingUpdateCallback = settingUpdate;
}

void ConfigManagerWeb::setEmbedWebUI(bool embed) {
    embedWebUI = embed;
}

void ConfigManagerWeb::setCustomHTML(const char* html, size_t length) {
    customHTML = html;
    customHTMLLen = length;
}

bool ConfigManagerWeb::isStarted() const {
    return initialized && server;
}

void ConfigManagerWeb::defineAllRoutes() {
    if (!initialized || !server) {
        WEB_LOG("[Web] Cannot define routes - not initialized");
        return;
    }
    
    setupStaticRoutes();
    setupAPIRoutes();
    setupRuntimeRoutes();
    
    WEB_LOG("[Web] All routes defined");
    
    // Start the server
    server->begin();
    WEB_LOG("[Web] Server started on port 80");
}

void ConfigManagerWeb::setupStaticRoutes() {
    // Root route
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRootRequest(request);
    });
    
    // CSS and JS routes
    server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleCSSRequest(request);
    });
    
    server->on("/script.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleJSRequest(request);
    });
    
    // Favicon
    server->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(404);
    });
    
    // 404 handler
    server->onNotFound([this](AsyncWebServerRequest* request) {
        handleNotFound(request);
    });
}

void ConfigManagerWeb::setupAPIRoutes() {
    // Configuration endpoints
    server->on("/config.json", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (configJsonProvider) {
            String json = configJsonProvider();
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
            enableCORS(response);
            request->send(response);
        } else {
            request->send(500, "application/json", "{\"error\":\"no_provider\"}");
        }
    });
    
    // Settings update endpoint
    server->on("/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
        bool hasGroup = request->hasParam("group", true);
        bool hasKey = request->hasParam("key", true);
        bool hasValue = request->hasParam("value", true);
        
        if (!hasGroup || !hasKey || !hasValue) {
            request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_params\"}");
            return;
        }
        
        String group = request->getParam("group", true)->value();
        String key = request->getParam("key", true)->value();
        String value = request->getParam("value", true)->value();
        
        if (settingUpdateCallback && settingUpdateCallback(group, key, value)) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"update_failed\"}");
        }
    });
    
    // Reset to defaults
    server->on("/config/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (resetCallback) {
            resetCallback();
            request->send(200, "application/json", "{\"status\":\"reset\"}");
        } else {
            request->send(500, "application/json", "{\"error\":\"no_callback\"}");
        }
    });
    
    // Reboot endpoint
    server->on("/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"status\":\"rebooting\"}");
        response->addHeader("Connection", "close");
        request->send(response);
        
        if (rebootCallback) {
            // Delay the reboot slightly to allow response to be sent
            rebootCallback();
        }
    });
}

void ConfigManagerWeb::setupRuntimeRoutes() {
    // Runtime data endpoints
    server->on("/runtime.json", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (runtimeJsonProvider) {
            String json = runtimeJsonProvider();
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
            enableCORS(response);
            request->send(response);
        } else {
            request->send(500, "application/json", "{\"error\":\"no_provider\"}");
        }
    });
    
    server->on("/runtime_meta.json", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (runtimeMetaJsonProvider) {
            String json = runtimeMetaJsonProvider();
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
            enableCORS(response);
            request->send(response);
        } else {
            request->send(500, "application/json", "{\"error\":\"no_provider\"}");
        }
    });
}

void ConfigManagerWeb::handleRootRequest(AsyncWebServerRequest* request) {
    if (customHTML && customHTMLLen > 0) {
        // Use custom HTML
        AsyncWebServerResponse* response = request->beginResponse_P(200, "text/html", (const uint8_t*)customHTML, customHTMLLen);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    } else if (embedWebUI) {
#if CM_EMBED_WEBUI
        // Use embedded WebUI
        WebHTML webhtml;
        const uint8_t* html = webhtml.getWebHTMLGz();
        size_t len = webhtml.getWebHTMLGzLen();
        AsyncWebServerResponse* response = request->beginResponse_P(200, "text/html", html, len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
#else
        request->send(500, "text/html", "<h1>WebUI not embedded</h1><p>Compile with CM_EMBED_WEBUI=1</p>");
#endif
    } else {
        request->send(404, "text/html", "<h1>No WebUI configured</h1>");
    }
}

void ConfigManagerWeb::handleCSSRequest(AsyncWebServerRequest* request) {
#if CM_EMBED_WEBUI
    // CSS is embedded in the main HTML file
    request->send(404, "text/css", "/* CSS embedded in HTML */");
#else
    request->send(404, "text/css", "/* CSS not embedded */");
#endif
}

void ConfigManagerWeb::handleJSRequest(AsyncWebServerRequest* request) {
#if CM_EMBED_WEBUI
    // JS is embedded in the main HTML file
    request->send(404, "application/javascript", "/* JS embedded in HTML */");
#else
    request->send(404, "application/javascript", "/* JS not embedded */");
#endif
}

void ConfigManagerWeb::handleNotFound(AsyncWebServerRequest* request) {
    WEB_LOG("[Web] 404: %s", request->url().c_str());
    request->send(404, "text/plain", "Not Found");
}

void ConfigManagerWeb::addCustomRoute(const char* path, WebRequestMethodComposite method, RequestHandler handler) {
    if (server) {
        server->on(path, method, handler);
    }
}

void ConfigManagerWeb::enableCORS(AsyncWebServerResponse* response) {
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

void ConfigManagerWeb::enableCORSForAll(bool enable) {
    if (enable && server) {
        server->onNotFound([this](AsyncWebServerRequest* request) {
            if (request->method() == HTTP_OPTIONS) {
                AsyncWebServerResponse* response = request->beginResponse(200);
                enableCORS(response);
                request->send(response);
            } else {
                handleNotFound(request);
            }
        });
    }
}

#ifdef development
void ConfigManagerWeb::addDevelopmentRoutes() {
    WEB_LOG("[Web] Adding development routes");
    
    // Development export route
    server->on("/dev/export", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (configJsonProvider) {
            String json = configJsonProvider();
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
            response->addHeader("Content-Disposition", "attachment; filename=\"config_export.json\"");
            enableCORS(response);
            request->send(response);
        } else {
            request->send(500, "application/json", "{\"error\":\"no_provider\"}");
        }
    });
}
#endif

String ConfigManagerWeb::getContentType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

void ConfigManagerWeb::log(const char* format, ...) const {
#if CM_ENABLE_LOGGING
    if (ConfigManagerClass_logger) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        ConfigManagerClass_logger(buffer);
    }
#endif
}