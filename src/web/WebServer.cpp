#include "WebServer.h"
#include "../ConfigManager.h"
#include "../settings.h"

#include <AsyncJson.h>

// Logging support
#define WEB_LOG(...) CM_LOG("[Web] " __VA_ARGS__)
#define WEB_LOG_VERBOSE(...) CM_LOG_VERBOSE("[Web] " __VA_ARGS__)

ConfigManagerWeb::ConfigManagerWeb(AsyncWebServer* webServer)
    : server(webServer)
    , configManager(nullptr)
    , initialized(false)
    , embedWebUI(CM_EMBED_WEBUI ? true : false)
    , customHTML(nullptr)
    , customHTMLLen(0)
    , settingsPassword("") // Empty by default - only protect if explicitly set
{
    if (!server) {
        server = new AsyncWebServer(80);
    }
}

ConfigManagerWeb::~ConfigManagerWeb() {
    // Don't delete server here as it might be externally managed
}

namespace
{
constexpr size_t kMaxJsonBodyBytes = 8 * 1024;
constexpr uintptr_t kRejectedBodyMarkerValue = 1;
constexpr const char* kAllocFailedJson = "{\"status\":\"error\",\"reason\":\"alloc_failed\"}";

void sendPayloadTooLarge(AsyncWebServerRequest* request)
{
    if (!request) {
        return;
    }
    request->send(413, "application/json", "{\"status\":\"error\",\"reason\":\"payload_too_large\"}");
}

void* rejectedBodyMarker()
{
    return reinterpret_cast<void*>(kRejectedBodyMarkerValue);
}

bool isRejectedBodyMarker(const void* ptr)
{
    return ptr == rejectedBodyMarker();
}

void clearRequestBodyBuffer(AsyncWebServerRequest* request)
{
    if (!request) {
        return;
    }

    if (request->_tempObject && !isRejectedBodyMarker(request->_tempObject)) {
        delete static_cast<String*>(request->_tempObject);
    }
    request->_tempObject = nullptr;
}

bool initRequestBodyBuffer(AsyncWebServerRequest* request, size_t expectedSize)
{
    if (!request) {
        return false;
    }

    if (expectedSize > kMaxJsonBodyBytes) {
        clearRequestBodyBuffer(request);
        request->_tempObject = rejectedBodyMarker();
        sendPayloadTooLarge(request);
        return false;
    }

    clearRequestBodyBuffer(request);
    String* body = new String();
    if (!body || !body->reserve(expectedSize)) {
        delete body;
        request->_tempObject = rejectedBodyMarker();
        request->send(500, "application/json", kAllocFailedJson);
        return false;
    }

    request->_tempObject = body;
    return true;
}

String* appendRequestBodyChunk(AsyncWebServerRequest* request, const uint8_t* data, size_t len, size_t index, size_t total)
{
    if (!request || isRejectedBodyMarker(request->_tempObject)) {
        return nullptr;
    }

    if (index == 0) {
        if (!initRequestBodyBuffer(request, total)) {
            return nullptr;
        }
    } else if (!request->_tempObject) {
        // Ignore follow-up chunks after a rejected/failed first chunk.
        return nullptr;
    }

    if (total > kMaxJsonBodyBytes) {
        clearRequestBodyBuffer(request);
        request->_tempObject = rejectedBodyMarker();
        return nullptr;
    }

    String* body = static_cast<String*>(request->_tempObject);
    if (!body) {
        return nullptr;
    }

    if ((body->length() + len) > kMaxJsonBodyBytes) {
        clearRequestBodyBuffer(request);
        request->_tempObject = rejectedBodyMarker();
        sendPayloadTooLarge(request);
        return nullptr;
    }

    if (!body->concat(reinterpret_cast<const char*>(data), len)) {
        clearRequestBodyBuffer(request);
        request->_tempObject = rejectedBodyMarker();
        request->send(500, "application/json", kAllocFailedJson);
        return nullptr;
    }

    return body;
}

bool parseForceFlag(AsyncWebServerRequest *request)
{
    if (!request)
    {
        return false;
    }
    AsyncWebParameter *param = nullptr;
    if (request->hasParam("force"))
    {
        param = request->getParam("force");
    }
    else if (request->hasParam("force", true))
    {
        param = request->getParam("force", true);
    }

    if (!param)
    {
        return false;
    }

    String value = param->value();
    value.trim();
    value.toLowerCase();
    return value == "1" || value == "true" || value == "yes";
}

class RequestContextScope
{
public:
    RequestContextScope(ConfigManagerClass *manager, const ConfigRequestContext &ctx)
        : manager(manager)
    {
        if (manager)
        {
            manager->pushRequestContext(ctx);
        }
    }

    ~RequestContextScope()
    {
        if (manager)
        {
            manager->popRequestContext();
        }
    }

private:
    ConfigManagerClass *manager = nullptr;
};
}

void ConfigManagerWeb::begin(ConfigManagerClass* cm) {
    configManager = cm;
    initialized = true;
    WEB_LOG("Web server module initialized");
}

void ConfigManagerWeb::setCallbacks(
    JsonProvider configJson,
    JsonProvider runtimeJson,
    JsonProvider runtimeMetaJson,
    SimpleCallback reboot,
    SimpleCallback reset,
    SettingUpdateCallback settingUpdate,
    SettingUpdateCallback settingApply
) {
    configJsonProvider = configJson;
    runtimeJsonProvider = runtimeJson;
    runtimeMetaJsonProvider = runtimeMetaJson;
    rebootCallback = reboot;
    resetCallback = reset;
    settingUpdateCallback = settingUpdate;
    settingApplyCallback = settingApply;
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
        WEB_LOG("Cannot define routes - not initialized");
        return;
    }

    setupStaticRoutes();
    setupAPIRoutes();
    setupRuntimeRoutes();

    WEB_LOG("All routes defined");

    // Start the server
    server->begin();
    WEB_LOG("Server started on port 80");
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

    // Optional user theme CSS injection
    server->on("/user_theme.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
        // Serve custom CSS if provided by the app via ConfigManager.setCustomCss().
        // If not provided, fall back to built-in default CSS (if compiled), else return empty.
        const char* css = nullptr;
        size_t len = 0;
        if (configManager) {
            css = configManager->getCustomCss();
            len = configManager->getCustomCssLen();
        }

        bool usingBuiltin = false;
#if defined(CM_DEFAULT_RUNTIME_STYLE_CSS)
        if (!css) {
            css = CM_DEFAULT_RUNTIME_STYLE_CSS;
            len = sizeof(CM_DEFAULT_RUNTIME_STYLE_CSS) - 1;
            usingBuiltin = true;
        }
#endif

        auto* response = request->beginResponseStream("text/css");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        if (css && (len > 0 || (usingBuiltin && css[0] != '\0'))) {
            if (len) {
                response->write(reinterpret_cast<const uint8_t*>(css), len);
            } else {
                response->print(css);
            }
        }
        request->send(response);
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
    // Simple version endpoint consumed by the UI header
    server->on("/version", HTTP_GET, [this](AsyncWebServerRequest* request) {
        String payload;
        if (configManager) {
            payload = configManager->getAppName();
        }
        if (payload.length() == 0) {
            payload = String("ConfigManager");  // Fallback if no app name set
        }
        if (payload.length()) payload += " ";
        
        // Get version from ConfigManager
        String version;
        if (configManager) {
            version = configManager->getVersion();
        }
        if (version.length() == 0) {
            version = String("");  // Fallback if no version set
        }
        payload += version;
        
        request->send(200, "text/plain", payload);
    });

    // App info endpoint (JSON) consumed by the WebUI.
    // Allows separate H1 (appName) and browser tab title (appTitle).
    server->on("/appinfo", HTTP_GET, [this](AsyncWebServerRequest* request) {
        DynamicJsonDocument out(256);
        out["appName"] = (configManager && configManager->getAppName().length()) ? configManager->getAppName() : String("");
        out["appTitle"] = (configManager && configManager->getAppTitle().length()) ? configManager->getAppTitle() : String("");
        out["version"] = (configManager && configManager->getVersion().length()) ? configManager->getVersion() : String("");
        out["guiLogging"] = (configManager && configManager->isGuiLoggingEnabled()) ? true : false;
        String resp;
        serializeJson(out, resp);

        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", resp);
        enableCORS(response);
        request->send(response);
    });
    // Debug route to catch any config requests with body handling
        server->on("/config_raw", HTTP_ANY,
        [this](AsyncWebServerRequest* request) {
            // Response sent in body handler for POST requests
            if (request->method() != HTTP_POST) {
                request->send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
            }
        },
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String* body = appendRequestBodyChunk(request, data, len, index, total);
            if (!body) {
                return;
            }

            if (index + len == total) {
                WEB_LOG_VERBOSE("config_raw done: params=%d bodyLen=%u",
                                request->params(), static_cast<unsigned>(body->length()));

                // Check for new format: category + key as URL params, value in JSON body
                bool hasCategory = request->hasParam("category");
                bool hasKey = request->hasParam("key");

                if (hasCategory && hasKey) {
                    String category = request->getParam("category")->value();
                    String key = request->getParam("key")->value();

                    // Parse JSON body to get value
                    DynamicJsonDocument doc(256);
                    DeserializationError err = deserializeJson(doc, *body);

                    String value;
                    if (!err && doc.containsKey("value")) {
                        if (doc["value"].is<String>()) {
                            value = doc["value"].as<String>();
                        } else {
                            // Convert other types to string
                            serializeJson(doc["value"], value);
                        }
                    } else {
                        // Fallback: use entire body as value
                        value = *body;
                        // Remove quotes if it's a JSON string
                        if (value.startsWith("\"") && value.endsWith("\"")) {
                            value = value.substring(1, value.length() - 1);
                        }
                    }

                    WEB_LOG_VERBOSE("config_raw parsed: %s.%s", category.c_str(), key.c_str());

                    // Passwords are transmitted in plaintext over HTTP.
                    String finalValue = value;
                    
                    if (settingUpdateCallback && settingUpdateCallback(category, key, finalValue)) {
                        request->send(200, "application/json", "{\"status\":\"ok\"}");
                    } else {
                        request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"update_failed\"}");
                    }
                } else {
                    WEB_LOG_VERBOSE("config_raw missing params: category=%s key=%s",
                                    hasCategory ? "yes" : "no", hasKey ? "yes" : "no");
                    request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_url_params\"}");
                }

                clearRequestBodyBuffer(request);
            }
        });

    // Configuration endpoints
    server->on("/config.json", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (configJsonProvider) {
            try {
                String json = configJsonProvider();
                size_t jsonSize = json.length();

                // Log the JSON size for debugging
                WEB_LOG_VERBOSE("config.json size=%u", (unsigned)jsonSize);

                // Check if JSON is empty or too large
                if (jsonSize == 0) {
                    WEB_LOG("Error: Generated JSON is empty!");
                    request->send(500, "application/json", "{\"error\":\"empty_json\"}");
                    return;
                }

                if (jsonSize > 16384) { // 16KB limit - use chunked response for large JSON
                    WEB_LOG_VERBOSE("config.json chunked response (%u)", (unsigned)jsonSize);
                    auto jsonShared = std::make_shared<String>(std::move(json));

                    AsyncWebServerResponse* response = request->beginChunkedResponse("application/json",
                        [jsonShared](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
                            if (!jsonShared || index >= jsonShared->length()) {
                                return 0;
                            }

                            size_t remaining = jsonShared->length() - index;
                            if (remaining == 0) return 0; // End of data

                            size_t chunkSize = min(maxLen, remaining);
                            memcpy(buffer, jsonShared->c_str() + index, chunkSize);
                            return chunkSize;
                        });
                    enableCORS(response);
                    request->send(response);
                } else {
                    // Normal response for smaller JSON
                    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
                    enableCORS(response);
                    request->send(response);
                }

                WEB_LOG_VERBOSE("config.json sent (%u)", (unsigned)jsonSize);
            } catch (const std::exception& e) {
                WEB_LOG("Exception in config.json: %s", e.what());
                request->send(500, "application/json", "{\"error\":\"json_generation_failed\"}");
            } catch (...) {
                WEB_LOG("Unknown exception in config.json generation");
                request->send(500, "application/json", "{\"error\":\"unknown_error\"}");
            }
        } else {
            request->send(500, "application/json", "{\"error\":\"no_provider\"}");
        }
    });

    server->on("/live_layout.json", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!configManager) {
            request->send(500, "application/json", "{\"error\":\"no_config\"}");
            return;
        }
        String json = configManager->buildLiveLayoutJSON();
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
        enableCORS(response);
        request->send(response);
    });

    // Settings update endpoint - /config/apply (matches frontend expectations)
    // Use AsyncCallbackJsonWebHandler to avoid edge cases with raw body accumulation.
    {
        auto* handler = new AsyncCallbackJsonWebHandler("/config/apply", [this](AsyncWebServerRequest* request, JsonVariant& json) {
            WEB_LOG_VERBOSE("/config/apply request");

            const String category = request->hasParam("category") ? request->getParam("category")->value() : "";
            const String key = request->hasParam("key") ? request->getParam("key")->value() : "";
            if (category.isEmpty() || key.isEmpty()) {
                WEB_LOG_VERBOSE("/config/apply missing URL params");
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json",
                    "{\"status\":\"error\",\"action\":\"apply\",\"reason\":\"missing_params\"}");
                enableCORS(response);
                request->send(response);
                return;
            }

            if (!json.is<JsonObject>()) {
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json",
                    "{\"status\":\"error\",\"action\":\"apply\",\"reason\":\"invalid_json\"}");
                enableCORS(response);
                request->send(response);
                return;
            }

            JsonObject obj = json.as<JsonObject>();
            String requestPayload;
            serializeJson(obj, requestPayload);
            if (!obj.containsKey("value")) {
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json",
                    "{\"status\":\"error\",\"action\":\"apply\",\"reason\":\"missing_value\"}");
                enableCORS(response);
                request->send(response);
                return;
            }

            String value;
            JsonVariant v = obj["value"];
            if (v.is<const char*>()) {
                value = v.as<String>();
            } else if (v.is<bool>()) {
                value = v.as<bool>() ? "true" : "false";
            } else if (v.is<int>()) {
                value = String(v.as<int>());
            } else if (v.is<float>()) {
                value = String(v.as<float>(), 6);
            } else {
                serializeJson(v, value);
            }

            WEB_LOG_VERBOSE("/config/apply %s.%s", category.c_str(), key.c_str());

            // Passwords are transmitted in plaintext over HTTP.
            const String finalValue = value;

            bool success = false;
            if (settingApplyCallback)
            {
                ConfigRequestContext ctx;
                ctx.origin = ConfigRequestContext::Origin::ApplySingle;
                ctx.endpoint = request->url();
                ctx.payload = requestPayload;
                ctx.force = parseForceFlag(request);
                RequestContextScope scope(configManager, ctx);
                success = settingApplyCallback(category, key, finalValue);
            }

            if (success) {
                const String payload = String("{\"status\":\"ok\",\"action\":\"apply\",\"category\":\"") +
                    category + "\",\"key\":\"" + key + "\"}";
                AsyncWebServerResponse* response = request->beginResponse(200, "application/json", payload);
                enableCORS(response);
                request->send(response);
            } else {
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json",
                    "{\"status\":\"error\",\"action\":\"apply\",\"reason\":\"update_failed\"}");
                enableCORS(response);
                request->send(response);
            }
        });
        handler->setMethod(HTTP_POST);
        server->addHandler(handler);
    }

        server->on("/gui/action", HTTP_POST, [this](AsyncWebServerRequest* request)
        {
            auto readParam = [request](const char *name) -> String
            {
                AsyncWebParameter *param = request->getParam(name, false);
                if (!param)
                {
                    param = request->getParam(name, true);
                }
                return param ? param->value() : String();
            };

            const String actionId = readParam("actionId");
            const String messageId = readParam("messageId");
            bool handled = false;
            if (configManager)
            {
                handled = configManager->handleGuiAction(messageId, actionId);
            }
            const String body = String("{\"status\":\"") + (handled ? "ok" : "error") + "\"}";
            AsyncWebServerResponse *response = request->beginResponse(handled ? 200 : 400, "application/json", body);
            enableCORS(response);
            request->send(response);
        });

    // Settings save endpoint - /config/save (saves individual setting to flash)
    // Use AsyncCallbackJsonWebHandler to avoid edge cases with chunked/unknown body sizes.
    {
        auto* handler = new AsyncCallbackJsonWebHandler("/config/save", [this](AsyncWebServerRequest* request, JsonVariant& json) {
            WEB_LOG_VERBOSE("/config/save request");

            const String category = request->hasParam("category") ? request->getParam("category")->value() : "";
            const String key = request->hasParam("key") ? request->getParam("key")->value() : "";
            if (category.isEmpty() || key.isEmpty()) {
                WEB_LOG_VERBOSE("/config/save missing URL params");
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json",
                    "{\"status\":\"error\",\"action\":\"save\",\"reason\":\"missing_params\"}");
                enableCORS(response);
                request->send(response);
                return;
            }

            if (!json.is<JsonObject>()) {
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json",
                    "{\"status\":\"error\",\"action\":\"save\",\"reason\":\"invalid_json\"}");
                enableCORS(response);
                request->send(response);
                return;
            }

            JsonObject obj = json.as<JsonObject>();
            String requestPayload;
            serializeJson(obj, requestPayload);
            String value;
            if (obj.containsKey("value")) {
                JsonVariant v = obj["value"];
                if (v.is<const char*>()) {
                    value = v.as<String>();
                } else if (v.is<bool>()) {
                    value = v.as<bool>() ? "true" : "false";
                } else if (v.is<int>()) {
                    value = String(v.as<int>());
                } else if (v.is<float>()) {
                    value = String(v.as<float>(), 6);
                } else {
                    serializeJson(v, value);
                }
            } else {
                // Backwards-compatible fallback
                value = "";
            }

            WEB_LOG("Processing /config/save: category='%s', key='%s'", category.c_str(), key.c_str());
            WEB_LOG("Extracted value for save: '%s'", value.c_str());

            // Passwords are transmitted in plaintext over HTTP.
            const String finalValue = value;

            bool success = false;
            if (settingUpdateCallback)
            {
                ConfigRequestContext ctx;
                ctx.origin = ConfigRequestContext::Origin::SaveSingle;
                ctx.endpoint = request->url();
                ctx.payload = requestPayload;
                ctx.force = parseForceFlag(request);
                RequestContextScope scope(configManager, ctx);
                success = settingUpdateCallback(category, key, finalValue);
            }

            if (success) {
                if (configManager) {
                    BaseSetting* setting = configManager->findSetting(category, key);
                    if (setting) {
                        WEB_LOG_VERBOSE("[D] Saved setting: %s.%s key=%s",
                                        setting->getCategory(),
                                        setting->getDisplayName(),
                                        setting->getKey());
                    } else {
                        WEB_LOG_VERBOSE("[D] Saved setting: %s.%s", category.c_str(), key.c_str());
                    }
                }
                const String payload = String("{\"status\":\"ok\",\"action\":\"save\",\"category\":\"") +
                    category + "\",\"key\":\"" + key + "\"}";
                AsyncWebServerResponse* response = request->beginResponse(200, "application/json", payload);
                enableCORS(response);
                request->send(response);
            } else {
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json",
                    "{\"status\":\"error\",\"action\":\"save\",\"reason\":\"update_failed\"}");
                enableCORS(response);
                request->send(response);
            }
        });
        handler->setMethod(HTTP_POST);
        server->addHandler(handler);
    }

    // Settings authentication endpoint - /config/auth
    // Issues a short-lived token required for password reveal.
    // Use AsyncCallbackJsonWebHandler to avoid edge cases with chunked/unknown body sizes.
    {
        auto* handler = new AsyncCallbackJsonWebHandler("/config/auth", [this](AsyncWebServerRequest* request, JsonVariant& json) {
            WEB_LOG_VERBOSE("/config/auth request");

            if (!json.is<JsonObject>()) {
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                enableCORS(response);
                request->send(response);
                return;
            }

            JsonObject obj = json.as<JsonObject>();
            const String provided = obj.containsKey("password") ? obj["password"].as<String>() : String("");
            const bool required = isSettingsAuthRequired();
            const bool match = (!required) || (provided == settingsPassword);
            WEB_LOG("/config/auth required=%s providedLen=%d configuredLen=%d match=%s",
                required ? "true" : "false",
                (int)provided.length(),
                (int)settingsPassword.length(),
                match ? "true" : "false");

            if (match) {
                const String token = issueSettingsAuthToken();
                DynamicJsonDocument out(256);
                out["status"] = "ok";
                out["token"] = token;
                out["ttlSec"] = (int)(SETTINGS_AUTH_TTL_MS / 1000UL);
                String resp;
                serializeJson(out, resp);

                AsyncWebServerResponse* response = request->beginResponse(200, "application/json", resp);
                enableCORS(response);
                request->send(response);
            } else {
                AsyncWebServerResponse* response = request->beginResponse(403, "application/json", "{\"status\":\"error\",\"reason\":\"unauthorized\"}");
                enableCORS(response);
                request->send(response);
            }
        });
        handler->setMethod(HTTP_POST);
        server->addHandler(handler);
    }

    // Password fetch endpoint - /config/password (returns actual password value)
    // Requires a valid auth token returned by /config/auth.
    server->on("/config/password", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!isSettingsAuthValid(request)) {
            request->send(403, "application/json", "{\"status\":\"error\",\"reason\":\"unauthorized\"}");
            return;
        }

        const String category = request->hasParam("category") ? request->getParam("category")->value() : "";
        const String key = request->hasParam("key") ? request->getParam("key")->value() : "";

        if (category.isEmpty() || key.isEmpty()) {
            request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_params\"}");
            return;
        }

        if (!configManager) {
            request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"not_initialized\"}");
            return;
        }

        BaseSetting* s = configManager->findSetting(category, key);
        if (!s || !s->isSecret()) {
            request->send(404, "application/json", "{\"status\":\"error\",\"reason\":\"not_found\"}");
            return;
        }

        if (s->getType() != SettingType::STRING) {
            request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"not_string_password\"}");
            return;
        }

        auto* cs = static_cast<Config<String>*>(s);

        DynamicJsonDocument out(512);
        out["status"] = "ok";
        out["value"] = cs->get();
        String resp;
        serializeJson(out, resp);
        request->send(200, "application/json", resp);
    });

    // Legacy endpoint (deprecated): /config/settings_password
    // Never return the configured password.
    server->on("/config/settings_password", HTTP_GET, [this](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(
            410,
            "application/json",
            "{\"status\":\"error\",\"reason\":\"deprecated\"}"
        );
        enableCORS(response);
        request->send(response);
    });

    // Bulk apply endpoint - /config/apply_all (applies all settings to memory only)
    // Use AsyncCallbackJsonWebHandler to avoid edge cases with chunked/unknown body sizes.
    {
        auto* handler = new AsyncCallbackJsonWebHandler("/config/apply_all", [this](AsyncWebServerRequest* request, JsonVariant& json) {
            WEB_LOG_VERBOSE("/config/apply_all request");

            if (!json.is<JsonObject>()) {
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json",
                    "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                enableCORS(response);
                request->send(response);
                return;
            }

            bool allSuccess = true;
            int totalApplied = 0;

            const bool forceFlag = parseForceFlag(request);
            JsonObject root = json.as<JsonObject>();
            for (JsonPair categoryPair : root) {
                const String category = categoryPair.key().c_str();
                if (!categoryPair.value().is<JsonObject>()) {
                    allSuccess = false;
                    continue;
                }

                JsonObject categoryObj = categoryPair.value().as<JsonObject>();
                for (JsonPair settingPair : categoryObj) {
                    const String key = settingPair.key().c_str();
                    const JsonVariant v = settingPair.value();

                    String value;
                    if (v.is<const char*>()) {
                        value = v.as<String>();
                    } else if (v.is<bool>()) {
                        value = v.as<bool>() ? "true" : "false";
                    } else if (v.is<int>()) {
                        value = String(v.as<int>());
                    } else if (v.is<float>()) {
                        value = String(v.as<float>(), 6);
                    } else {
                        serializeJson(v, value);
                    }

                    String requestPayload;
                    {
                        StaticJsonDocument<128> payloadDoc;
                        payloadDoc["value"] = v;
                        serializeJson(payloadDoc, requestPayload);
                    }

                    bool callResult = false;
                    if (settingApplyCallback)
                    {
                        ConfigRequestContext ctx;
                        ctx.origin = ConfigRequestContext::Origin::ApplyAll;
                        ctx.endpoint = request->url();
                        ctx.payload = requestPayload;
                        ctx.force = forceFlag;
                        RequestContextScope scope(configManager, ctx);
                        callResult = settingApplyCallback(category, key, value);
                    }

                    if (callResult) {
                        totalApplied++;
                        WEB_LOG("Applied %s.%s = %s", category.c_str(), key.c_str(), value.c_str());
                    } else {
                        allSuccess = false;
                        WEB_LOG("Failed to apply %s.%s = %s", category.c_str(), key.c_str(), value.c_str());
                    }
                }
            }

            if (allSuccess && totalApplied > 0) {
                const String payload = String("{\"status\":\"ok\",\"action\":\"apply_all\",\"applied\":") + String(totalApplied) + "}";
                AsyncWebServerResponse* response = request->beginResponse(200, "application/json", payload);
                enableCORS(response);
                request->send(response);
            } else {
                const String payload = String("{\"status\":\"error\",\"action\":\"apply_all\",\"applied\":") + String(totalApplied) + "}";
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json", payload);
                enableCORS(response);
                request->send(response);
            }
        });
        handler->setMethod(HTTP_POST);
        server->addHandler(handler);
    }

    // Bulk save endpoint - /config/save_all (saves all settings to flash)
    // Use AsyncCallbackJsonWebHandler to avoid edge cases with chunked/unknown body sizes.
    {
        auto* handler = new AsyncCallbackJsonWebHandler("/config/save_all", [this](AsyncWebServerRequest* request, JsonVariant& json) {
            WEB_LOG("Processing /config/save_all");

            if (!json.is<JsonObject>()) {
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json",
                    "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                enableCORS(response);
                request->send(response);
                return;
            }

            bool allSuccess = true;
            int totalSaved = 0;

            const bool forceFlag = parseForceFlag(request);
            JsonObject root = json.as<JsonObject>();
            for (JsonPair categoryPair : root) {
                const String category = categoryPair.key().c_str();
                if (!categoryPair.value().is<JsonObject>()) {
                    allSuccess = false;
                    continue;
                }

                JsonObject categoryObj = categoryPair.value().as<JsonObject>();
                for (JsonPair settingPair : categoryObj) {
                    const String key = settingPair.key().c_str();
                    const JsonVariant v = settingPair.value();

                    String value;
                    if (v.is<const char*>()) {
                        value = v.as<String>();
                    } else if (v.is<bool>()) {
                        value = v.as<bool>() ? "true" : "false";
                    } else if (v.is<int>()) {
                        value = String(v.as<int>());
                    } else if (v.is<float>()) {
                        value = String(v.as<float>(), 6);
                    } else {
                        serializeJson(v, value);
                    }

                    String requestPayload;
                    {
                        StaticJsonDocument<128> payloadDoc;
                        payloadDoc["value"] = v;
                        serializeJson(payloadDoc, requestPayload);
                    }

                    bool callResult = false;
                    if (settingUpdateCallback)
                    {
                        ConfigRequestContext ctx;
                        ctx.origin = ConfigRequestContext::Origin::SaveAll;
                        ctx.endpoint = request->url();
                        ctx.payload = requestPayload;
                        ctx.force = forceFlag;
                        RequestContextScope scope(configManager, ctx);
                        callResult = settingUpdateCallback(category, key, value);
                    }

                    if (callResult) {
                        totalSaved++;
                        WEB_LOG("Saved %s.%s = %s", category.c_str(), key.c_str(), value.c_str());
                    } else {
                        allSuccess = false;
                        WEB_LOG("Failed to save %s.%s = %s", category.c_str(), key.c_str(), value.c_str());
                    }
                }
            }

            if (allSuccess && totalSaved > 0) {
                const String payload = String("{\"status\":\"ok\",\"action\":\"save_all\",\"saved\":") + String(totalSaved) + "}";
                AsyncWebServerResponse* response = request->beginResponse(200, "application/json", payload);
                enableCORS(response);
                request->send(response);
            } else {
                const String payload = String("{\"status\":\"error\",\"action\":\"save_all\",\"saved\":") + String(totalSaved) + "}";
                AsyncWebServerResponse* response = request->beginResponse(400, "application/json", payload);
                enableCORS(response);
                request->send(response);
            }
        });
        handler->setMethod(HTTP_POST);
        server->addHandler(handler);
    }

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
        request->send(404, "text/html", "<h1>WebUI not embedded</h1><p>This firmware was built with CM_EMBED_WEBUI=0</p>");
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
    WEB_LOG("404: %s %s",
            request->methodToString(),
            request->url().c_str());
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
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Settings-Token");
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
    WEB_LOG("Adding development routes");

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

void ConfigManagerWeb::setupRuntimeRoutes() {
    WEB_LOG("Setting up runtime routes");

    // Runtime JSON endpoint
    server->on("/runtime.json", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (runtimeJsonProvider) {
            String json = runtimeJsonProvider();
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
            enableCORS(response);
            response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            response->addHeader("Pragma", "no-cache");
            response->addHeader("Expires", "0");
            response->addHeader("Connection", "close");
            request->send(response);
        } else {
            request->send(500, "application/json", "{\"error\":\"no_provider\"}");
        }
    });

    // Runtime metadata endpoint
    server->on("/runtime_meta.json", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (runtimeMetaJsonProvider) {
            String json = runtimeMetaJsonProvider();
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
            enableCORS(response);
            response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            response->addHeader("Pragma", "no-cache");
            response->addHeader("Expires", "0");
            response->addHeader("Connection", "close");
            request->send(response);
        } else {
            request->send(500, "application/json", "{\"error\":\"no_provider\"}");
        }
    });

#if !CM_ENABLE_WS_PUSH
    // Gracefully handle WebSocket requests when WS push is disabled to avoid 404 log spam
    server->on("/ws", HTTP_GET, [this](AsyncWebServerRequest* request) {
        // If the client attempts a WebSocket handshake, we deliberately respond with 426
        // so the browser treats it as an upgrade-required failure (no noisy 404 logging).
        AsyncWebServerResponse* response = request->beginResponse(426, "text/plain", "WebSocket disabled");
        response->addHeader("Connection", "close");
        response->addHeader("Sec-WebSocket-Version", "13");
        request->send(response);
    });
#endif

    // Runtime button press endpoint
    server->on("/runtime_action/button", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!configManager) {
                request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                return;
            }

            // Check for query parameters first (frontend uses this method)
            if (request->hasParam("group") && request->hasParam("key")) {
                String group = request->getParam("group")->value();
                String key = request->getParam("key")->value();

                configManager->getRuntimeManager().handleButtonPress(group, key);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }

            // Fallback to JSON body parsing
            if (!initRequestBodyBuffer(request, request->contentLength())) {
                return;
            }
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String* body = appendRequestBodyChunk(request, data, len, index, total);
            if (!body) {
                return;
            }

            if (index + len == total) {
                    DynamicJsonDocument doc(256);
                    DeserializationError error = deserializeJson(doc, *body);

                    if (!error && doc.containsKey("group") && doc.containsKey("key")) {
                        String group = doc["group"].as<String>();
                        String key = doc["key"].as<String>();

                        if (configManager) {
                            configManager->getRuntimeManager().handleButtonPress(group, key);
                            request->send(200, "application/json", "{\"status\":\"ok\"}");
                        } else {
                            request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                        }
                    } else {
                        request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                    }

                    clearRequestBodyBuffer(request);
            }
        });

    // Runtime checkbox change endpoint
    server->on("/runtime_action/checkbox", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!configManager) {
                request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                return;
            }

            // Check for query parameters first (frontend uses this method)
            if (request->hasParam("group") && request->hasParam("key") && request->hasParam("value")) {
                String group = request->getParam("group")->value();
                String key = request->getParam("key")->value();
                String valueStr = request->getParam("value")->value();
                bool value = (valueStr == "true" || valueStr == "1");

                configManager->getRuntimeManager().handleCheckboxChange(group, key, value);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }

            // Fallback to JSON body parsing
            if (!initRequestBodyBuffer(request, request->contentLength())) {
                return;
            }
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String* body = appendRequestBodyChunk(request, data, len, index, total);
            if (!body) {
                return;
            }

            if (index + len == total) {
                    DynamicJsonDocument doc(256);
                    DeserializationError error = deserializeJson(doc, *body);

                    if (!error && doc.containsKey("group") && doc.containsKey("key") && doc.containsKey("value")) {
                        String group = doc["group"].as<String>();
                        String key = doc["key"].as<String>();
                        bool value = doc["value"].as<bool>();

                        if (configManager) {
                            configManager->getRuntimeManager().handleCheckboxChange(group, key, value);
                            request->send(200, "application/json", "{\"status\":\"ok\"}");
                        } else {
                            request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                        }
                    } else {
                        request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                    }

                    clearRequestBodyBuffer(request);
            }
        });

    // Runtime state button endpoint (toggle or explicit set via value=...)
    server->on("/runtime_action/state_button", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!configManager) {
                request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                return;
            }

            // Check for query parameters first (frontend uses this method)
            if (request->hasParam("group") && request->hasParam("key")) {
                String group = request->getParam("group")->value();
                String key = request->getParam("key")->value();

                if (request->hasParam("value")) {
                    String raw = request->getParam("value")->value();
                    raw.toLowerCase();
                    const bool value = (raw == "true" || raw == "1" || raw == "on");
                    configManager->getRuntimeManager().handleStateButtonSet(group, key, value);
                } else {
                    configManager->getRuntimeManager().handleStateButtonToggle(group, key);
                }
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }

            // Fallback to JSON body parsing
            if (!initRequestBodyBuffer(request, request->contentLength())) {
                return;
            }
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String* body = appendRequestBodyChunk(request, data, len, index, total);
            if (!body) {
                return;
            }

            if (index + len == total) {
                    DynamicJsonDocument doc(256);
                    DeserializationError error = deserializeJson(doc, *body);

                    if (!error && doc.containsKey("group") && doc.containsKey("key")) {
                        String group = doc["group"].as<String>();
                        String key = doc["key"].as<String>();

                        if (configManager) {
                            if (doc.containsKey("value")) {
                                bool value = doc["value"].as<bool>();
                                configManager->getRuntimeManager().handleStateButtonSet(group, key, value);
                            } else {
                                configManager->getRuntimeManager().handleStateButtonToggle(group, key);
                            }
                            request->send(200, "application/json", "{\"status\":\"ok\"}");
                        } else {
                            request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                        }
                    } else {
                        request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                    }

                    clearRequestBodyBuffer(request);
            }
        });

    // Runtime int slider change endpoint
    server->on("/runtime_action/int_slider", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!configManager) {
                request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                return;
            }

            // Check for query parameters first (frontend uses this method)
            if (request->hasParam("group") && request->hasParam("key") && request->hasParam("value")) {
                String group = request->getParam("group")->value();
                String key = request->getParam("key")->value();
                String valueStr = request->getParam("value")->value();
                int value = valueStr.toInt();

                configManager->getRuntimeManager().handleIntSliderChange(group, key, value);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }

            // Fallback to JSON body parsing
            if (!initRequestBodyBuffer(request, request->contentLength())) {
                return;
            }
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String* body = appendRequestBodyChunk(request, data, len, index, total);
            if (!body) {
                return;
            }

            if (index + len == total) {
                    DynamicJsonDocument doc(256);
                    DeserializationError error = deserializeJson(doc, *body);

                    if (!error && doc.containsKey("group") && doc.containsKey("key") && doc.containsKey("value")) {
                        String group = doc["group"].as<String>();
                        String key = doc["key"].as<String>();
                        int value = doc["value"].as<int>();

                        if (configManager) {
                            configManager->getRuntimeManager().handleIntSliderChange(group, key, value);
                            request->send(200, "application/json", "{\"status\":\"ok\"}");
                        } else {
                            request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                        }
                    } else {
                        request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                    }

                    clearRequestBodyBuffer(request);
            }
        });

    // Runtime float slider change endpoint
    server->on("/runtime_action/float_slider", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!configManager) {
                request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                return;
            }

            // Check for query parameters first (frontend uses this method)
            if (request->hasParam("group") && request->hasParam("key") && request->hasParam("value")) {
                String group = request->getParam("group")->value();
                String key = request->getParam("key")->value();
                String valueStr = request->getParam("value")->value();
                
                // Handle European decimal format (comma instead of dot)
                valueStr.replace(",", ".");
                float value = valueStr.toFloat();

                configManager->getRuntimeManager().handleFloatSliderChange(group, key, value);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }

            // Fallback to JSON body parsing
            if (!initRequestBodyBuffer(request, request->contentLength())) {
                return;
            }
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String* body = appendRequestBodyChunk(request, data, len, index, total);
            if (!body) {
                return;
            }

            if (index + len == total) {
                    DynamicJsonDocument doc(256);
                    DeserializationError error = deserializeJson(doc, *body);

                    if (!error && doc.containsKey("group") && doc.containsKey("key") && doc.containsKey("value")) {
                        String group = doc["group"].as<String>();
                        String key = doc["key"].as<String>();
                        float value = doc["value"].as<float>();

                        if (configManager) {
                            configManager->getRuntimeManager().handleFloatSliderChange(group, key, value);
                            request->send(200, "application/json", "{\"status\":\"ok\"}");
                        } else {
                            request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                        }
                    } else {
                        request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                    }

                    clearRequestBodyBuffer(request);
            }
        });

    // Runtime int value input endpoint
    server->on("/runtime_action/int_input", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!configManager) {
                request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                return;
            }

            if (request->hasParam("group") && request->hasParam("key") && request->hasParam("value")) {
                String group = request->getParam("group")->value();
                String key = request->getParam("key")->value();
                String valueStr = request->getParam("value")->value();
                int value = valueStr.toInt();

                configManager->getRuntimeManager().handleIntInputChange(group, key, value);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }

            if (!initRequestBodyBuffer(request, request->contentLength())) {
                return;
            }
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String* body = appendRequestBodyChunk(request, data, len, index, total);
            if (!body) {
                return;
            }

            if (index + len == total) {
                    DynamicJsonDocument doc(256);
                    DeserializationError error = deserializeJson(doc, *body);

                    if (!error && doc.containsKey("group") && doc.containsKey("key") && doc.containsKey("value")) {
                        String group = doc["group"].as<String>();
                        String key = doc["key"].as<String>();
                        int value = doc["value"].as<int>();

                        if (configManager) {
                            configManager->getRuntimeManager().handleIntInputChange(group, key, value);
                            request->send(200, "application/json", "{\"status\":\"ok\"}");
                        } else {
                            request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                        }
                    } else {
                        request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                    }

                    clearRequestBodyBuffer(request);
            }
        });

    // Runtime float value input endpoint
    server->on("/runtime_action/float_input", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            if (!configManager) {
                request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                return;
            }

            if (request->hasParam("group") && request->hasParam("key") && request->hasParam("value")) {
                String group = request->getParam("group")->value();
                String key = request->getParam("key")->value();
                String valueStr = request->getParam("value")->value();
                valueStr.replace(",", ".");
                float value = valueStr.toFloat();

                configManager->getRuntimeManager().handleFloatInputChange(group, key, value);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }

            if (!initRequestBodyBuffer(request, request->contentLength())) {
                return;
            }
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String* body = appendRequestBodyChunk(request, data, len, index, total);
            if (!body) {
                return;
            }

            if (index + len == total) {
                    DynamicJsonDocument doc(256);
                    DeserializationError error = deserializeJson(doc, *body);

                    if (!error && doc.containsKey("group") && doc.containsKey("key") && doc.containsKey("value")) {
                        String group = doc["group"].as<String>();
                        String key = doc["key"].as<String>();
                        float value = doc["value"].as<float>();

                        if (configManager) {
                            configManager->getRuntimeManager().handleFloatInputChange(group, key, value);
                            request->send(200, "application/json", "{\"status\":\"ok\"}");
                        } else {
                            request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                        }
                    } else {
                        request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                    }

                    clearRequestBodyBuffer(request);
            }
        });
}

String ConfigManagerWeb::getContentType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

void ConfigManagerWeb::log(const char* format, ...) const {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    WEB_LOG("%s", buffer);
}

void ConfigManagerWeb::setSettingsPassword(const String& password) {
    settingsPassword = password;
    WEB_LOG("Settings password configured (length: %d)", password.length());
}

bool ConfigManagerWeb::isSettingsAuthRequired() const {
    return settingsPassword.length() > 0;
}

String ConfigManagerWeb::issueSettingsAuthToken() {
    // 128-bit token encoded as hex
    uint32_t r0 = esp_random();
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();
    char buf[33];
    snprintf(buf, sizeof(buf), "%08lx%08lx%08lx%08lx",
             (unsigned long)r0, (unsigned long)r1, (unsigned long)r2, (unsigned long)r3);
    settingsAuthToken = String(buf);
    settingsAuthIssuedAtMs = millis();
    WEB_LOG("Settings auth token issued (ttl=%lus)", (unsigned long)(SETTINGS_AUTH_TTL_MS / 1000UL));
    return settingsAuthToken;
}

bool ConfigManagerWeb::isSettingsAuthValid(AsyncWebServerRequest* request) const {
    if (!isSettingsAuthRequired()) {
        return true;
    }

    if (settingsAuthToken.isEmpty()) {
        return false;
    }

    const uint32_t age = (uint32_t)(millis() - settingsAuthIssuedAtMs);
    if (age > SETTINGS_AUTH_TTL_MS) {
        return false;
    }

    if (!request || !request->hasHeader("X-Settings-Token")) {
        return false;
    }
    const String token = request->getHeader("X-Settings-Token")->value();
    return token == settingsAuthToken;
}

