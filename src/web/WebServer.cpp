#include "WebServer.h"
#include "../ConfigManager.h"
#include "../settings.h"

#include <AsyncJson.h>

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
    , settingsPassword("") // Empty by default - only protect if explicitly set
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
            // Handle POST body for /config endpoint
            if (index == 0) {
                request->_tempObject = new String();
                static_cast<String*>(request->_tempObject)->reserve(total);
            }

                String* body = static_cast<String*>(request->_tempObject);
            body->concat(String((const char*)data).substring(0, len));

            if (index + len == total) {
                WEB_LOG("[Web] POST /config with body");

                // Debug: List all parameters
                WEB_LOG("[Web] Total params: %d", request->params());
                for (int i = 0; i < request->params(); i++) {
                    AsyncWebParameter* p = request->getParam(i);
                    WEB_LOG("[Web] Param %d: name='%s', value='%s', isPost=%s, isFile=%s",
                            i, p->name().c_str(), p->value().c_str(),
                            p->isPost() ? "true" : "false", p->isFile() ? "true" : "false");
                }

                WEB_LOG("[Web] Body content: '%s'", body->c_str());

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

                    WEB_LOG("[Web] Parsed: category='%s', key='%s', value='%s'",
                            category.c_str(), key.c_str(), value.c_str());

                    // Passwords are transmitted in plaintext over HTTP.
                    String finalValue = value;
                    
                    if (settingUpdateCallback && settingUpdateCallback(category, key, finalValue)) {
                        request->send(200, "application/json", "{\"status\":\"ok\"}");
                    } else {
                        request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"update_failed\"}");
                    }
                } else {
                    WEB_LOG("[Web] Missing URL params: category=%s, key=%s",
                            hasCategory ? "yes" : "no", hasKey ? "yes" : "no");
                    request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_url_params\"}");
                }

                delete body;
                request->_tempObject = nullptr;
            }
        });

    // Configuration endpoints
    server->on("/config.json", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (configJsonProvider) {
            try {
                String json = configJsonProvider();
                size_t jsonSize = json.length();

                // Log the JSON size for debugging
                Serial.printf("[WebServer] Sending config.json - Size: %zu bytes\n", jsonSize);

                // Check if JSON is empty or too large
                if (jsonSize == 0) {
                    Serial.println("[WebServer] Error: Generated JSON is empty!");
                    request->send(500, "application/json", "{\"error\":\"empty_json\"}");
                    return;
                }

                if (jsonSize > 16384) { // 16KB limit - use chunked response for large JSON
                    Serial.printf("[WebServer] Using chunked response for large JSON (%zu bytes)\n", jsonSize);

                    AsyncWebServerResponse* response = request->beginChunkedResponse("application/json",
                        [json](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
                            size_t remaining = json.length() - index;
                            if (remaining == 0) return 0; // End of data

                            size_t chunkSize = min(maxLen, remaining);
                            memcpy(buffer, json.c_str() + index, chunkSize);
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

                Serial.printf("[WebServer] config.json sent successfully (%zu bytes)\n", jsonSize);
            } catch (const std::exception& e) {
                Serial.printf("[WebServer] Exception in config.json: %s\n", e.what());
                request->send(500, "application/json", "{\"error\":\"json_generation_failed\"}");
            } catch (...) {
                Serial.println("[WebServer] Unknown exception in config.json generation");
                request->send(500, "application/json", "{\"error\":\"unknown_error\"}");
            }
        } else {
            request->send(500, "application/json", "{\"error\":\"no_provider\"}");
        }
    });

    // Settings update endpoint - /config/apply (matches frontend expectations)
    server->on("/config/apply", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            // Response is sent in body handler
            Serial.println("[WebServer] /config/apply request received");
        },
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            // Accumulate body data
            if (index == 0) {
                request->_tempObject = new String();
                static_cast<String*>(request->_tempObject)->reserve(total);
            }

            String* body = static_cast<String*>(request->_tempObject);
            body->concat(String((const char*)data).substring(0, len));

            // Process when all data received
            if (index + len == total) {
                Serial.printf("[WebServer] /config/apply body complete (%u bytes)\n", (unsigned)total);
                // Get URL parameters
                AsyncWebParameter* pCategory = request->getParam("category");
                AsyncWebParameter* pKey = request->getParam("key");

                if (!pCategory || !pKey) {
                    WEB_LOG("[Web] Missing URL params for /config/apply");
                    request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_params\"}");
                    delete body;
                    request->_tempObject = nullptr;
                    return;
                }

                String category = pCategory->value();
                String key = pKey->value();

                WEB_LOG("[Web] Processing /config/apply: category='%s', key='%s'",
                        category.c_str(), key.c_str());

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
                }

                WEB_LOG("[Web] Extracted value: '%s'", value.c_str());

                // Passwords are transmitted in plaintext over HTTP.
                String finalValue = value;
                
                // Call apply callback (memory only, no flash save)
                if (settingApplyCallback && settingApplyCallback(category, key, finalValue)) {
                    String response = "{\"status\":\"ok\",\"action\":\"apply\",\"category\":\"" +
                                    category + "\",\"key\":\"" + key + "\"}";
                    request->send(200, "application/json", response);
                } else {
                    request->send(400, "application/json",
                                "{\"status\":\"error\",\"action\":\"apply\",\"reason\":\"update_failed\"}");
                }

                delete body;
                request->_tempObject = nullptr;
            }
        });

    // Settings save endpoint - /config/save (saves individual setting to flash)
    // Use AsyncCallbackJsonWebHandler to avoid edge cases with chunked/unknown body sizes.
    {
        auto* handler = new AsyncCallbackJsonWebHandler("/config/save", [this](AsyncWebServerRequest* request, JsonVariant& json) {
            WEB_LOG("[WebServer] /config/save request received");

            const String category = request->hasParam("category") ? request->getParam("category")->value() : "";
            const String key = request->hasParam("key") ? request->getParam("key")->value() : "";
            if (category.isEmpty() || key.isEmpty()) {
                WEB_LOG("[Web] Missing URL params for /config/save");
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

            WEB_LOG("[Web] Processing /config/save: category='%s', key='%s'", category.c_str(), key.c_str());
            WEB_LOG("[Web] Extracted value for save: '%s'", value.c_str());

            // Passwords are transmitted in plaintext over HTTP.
            const String finalValue = value;

            if (settingUpdateCallback && settingUpdateCallback(category, key, finalValue)) {
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
            WEB_LOG("[WebServer] /config/auth request received");

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
            WEB_LOG("[Web] /config/auth required=%s providedLen=%d configuredLen=%d match=%s",
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

    // Settings password endpoint - /config/settings_password (returns settings password for frontend auth)
    server->on("/config/settings_password", HTTP_GET, [this](AsyncWebServerRequest* request) {
        String response = "{\"status\":\"ok\",\"password\":\"" + settingsPassword + "\"}";
        request->send(200, "application/json", response);
    });

    // Bulk apply endpoint - /config/apply_all (applies all settings to memory only)
    server->on("/config/apply_all", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            request->_tempObject = new String();
        },
        nullptr, // upload handler
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (request->_tempObject) {
                String* body = static_cast<String*>(request->_tempObject);
                for (size_t i = 0; i < len; i++) {
                    *body += (char)data[i];
                }

                if (index + len == total) {
                    WEB_LOG("[Web] Processing /config/apply_all");

                    // Parse JSON body containing all settings
                    DynamicJsonDocument doc(4096);
                    DeserializationError error = deserializeJson(doc, *body);

                    if (error) {
                        WEB_LOG("[Web] JSON parse error in apply_all: %s", error.c_str());
                        request->send(400, "application/json",
                                    "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                        delete body;
                        request->_tempObject = nullptr;
                        return;
                    }

                    bool allSuccess = true;
                    int totalApplied = 0;

                    // Iterate through categories
                    for (JsonPair categoryPair : doc.as<JsonObject>()) {
                        String category = categoryPair.key().c_str();
                        JsonObject categoryObj = categoryPair.value().as<JsonObject>();

                        // Iterate through settings in category
                        for (JsonPair settingPair : categoryObj) {
                            String key = settingPair.key().c_str();
                            String value;

                            // Convert value to string
                            if (settingPair.value().is<String>()) {
                                value = settingPair.value().as<String>();
                            } else if (settingPair.value().is<bool>()) {
                                value = settingPair.value().as<bool>() ? "true" : "false";
                            } else if (settingPair.value().is<int>()) {
                                value = String(settingPair.value().as<int>());
                            } else if (settingPair.value().is<float>()) {
                                value = String(settingPair.value().as<float>(), 6);
                            }

                            // Apply setting (memory only)
                            if (settingApplyCallback && settingApplyCallback(category, key, value)) {
                                totalApplied++;
                                WEB_LOG("[Web] Applied %s.%s = %s", category.c_str(), key.c_str(), value.c_str());
                            } else {
                                allSuccess = false;
                                WEB_LOG("[Web] Failed to apply %s.%s = %s", category.c_str(), key.c_str(), value.c_str());
                            }
                        }
                    }

                    // Send response
                    if (allSuccess && totalApplied > 0) {
                        String response = "{\"status\":\"ok\",\"action\":\"apply_all\",\"applied\":" + String(totalApplied) + "}";
                        request->send(200, "application/json", response);
                    } else {
                        String response = "{\"status\":\"error\",\"action\":\"apply_all\",\"applied\":" + String(totalApplied) + "}";
                        request->send(400, "application/json", response);
                    }

                    delete body;
                    request->_tempObject = nullptr;
                }
            }
        });

    // Bulk save endpoint - /config/save_all (saves all settings to flash)
    server->on("/config/save_all", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            request->_tempObject = new String();
        },
        nullptr, // upload handler
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (request->_tempObject) {
                String* body = static_cast<String*>(request->_tempObject);
                for (size_t i = 0; i < len; i++) {
                    *body += (char)data[i];
                }

                if (index + len == total) {
                    WEB_LOG("[Web] Processing /config/save_all");

                    // Parse JSON body containing all settings
                    DynamicJsonDocument doc(4096);
                    DeserializationError error = deserializeJson(doc, *body);

                    if (error) {
                        WEB_LOG("[Web] JSON parse error in save_all: %s", error.c_str());
                        request->send(400, "application/json",
                                    "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                        delete body;
                        request->_tempObject = nullptr;
                        return;
                    }

                    bool allSuccess = true;
                    int totalSaved = 0;

                    // Iterate through categories
                    for (JsonPair categoryPair : doc.as<JsonObject>()) {
                        String category = categoryPair.key().c_str();
                        JsonObject categoryObj = categoryPair.value().as<JsonObject>();

                        // Iterate through settings in category
                        for (JsonPair settingPair : categoryObj) {
                            String key = settingPair.key().c_str();
                            String value;

                            // Convert value to string
                            if (settingPair.value().is<String>()) {
                                value = settingPair.value().as<String>();
                            } else if (settingPair.value().is<bool>()) {
                                value = settingPair.value().as<bool>() ? "true" : "false";
                            } else if (settingPair.value().is<int>()) {
                                value = String(settingPair.value().as<int>());
                            } else if (settingPair.value().is<float>()) {
                                value = String(settingPair.value().as<float>(), 6);
                            }

                            // Save setting (memory + flash)
                            if (settingUpdateCallback && settingUpdateCallback(category, key, value)) {
                                totalSaved++;
                                WEB_LOG("[Web] Saved %s.%s = %s", category.c_str(), key.c_str(), value.c_str());
                            } else {
                                allSuccess = false;
                                WEB_LOG("[Web] Failed to save %s.%s = %s", category.c_str(), key.c_str(), value.c_str());
                            }
                        }
                    }

                    // Send response
                    if (allSuccess && totalSaved > 0) {
                        String response = "{\"status\":\"ok\",\"action\":\"save_all\",\"saved\":" + String(totalSaved) + "}";
                        request->send(200, "application/json", response);
                    } else {
                        String response = "{\"status\":\"error\",\"action\":\"save_all\",\"saved\":" + String(totalSaved) + "}";
                        request->send(400, "application/json", response);
                    }

                    delete body;
                    request->_tempObject = nullptr;
                }
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
    WEB_LOG("[Web] 404: %s %s",
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

void ConfigManagerWeb::setupRuntimeRoutes() {
    WEB_LOG("[Web] Setting up runtime routes");

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

#if CM_ENABLE_RUNTIME_BUTTONS
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
            request->_tempObject = new String();
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (request->_tempObject) {
                String* body = static_cast<String*>(request->_tempObject);
                for (size_t i = 0; i < len; i++) {
                    *body += (char)data[i];
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

                    delete body;
                    request->_tempObject = nullptr;
                }
            }
        });
#endif

#if CM_ENABLE_RUNTIME_CHECKBOXES
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
            request->_tempObject = new String();
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (request->_tempObject) {
                String* body = static_cast<String*>(request->_tempObject);
                for (size_t i = 0; i < len; i++) {
                    *body += (char)data[i];
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

                    delete body;
                    request->_tempObject = nullptr;
                }
            }
        });
#endif

#if CM_ENABLE_RUNTIME_STATE_BUTTONS
    // Runtime state button toggle endpoint
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

                configManager->getRuntimeManager().handleStateButtonToggle(group, key);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }

            // Fallback to JSON body parsing
            request->_tempObject = new String();
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (request->_tempObject) {
                String* body = static_cast<String*>(request->_tempObject);
                for (size_t i = 0; i < len; i++) {
                    *body += (char)data[i];
                }

                if (index + len == total) {
                    DynamicJsonDocument doc(256);
                    DeserializationError error = deserializeJson(doc, *body);

                    if (!error && doc.containsKey("group") && doc.containsKey("key")) {
                        String group = doc["group"].as<String>();
                        String key = doc["key"].as<String>();

                        if (configManager) {
                            configManager->getRuntimeManager().handleStateButtonToggle(group, key);
                            request->send(200, "application/json", "{\"status\":\"ok\"}");
                        } else {
                            request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_manager\"}");
                        }
                    } else {
                        request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                    }

                    delete body;
                    request->_tempObject = nullptr;
                }
            }
        });
#endif

#if (CM_ENABLE_RUNTIME_INT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
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
            request->_tempObject = new String();
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (request->_tempObject) {
                String* body = static_cast<String*>(request->_tempObject);
                for (size_t i = 0; i < len; i++) {
                    *body += (char)data[i];
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

                    delete body;
                    request->_tempObject = nullptr;
                }
            }
        });
#endif

#if (CM_ENABLE_RUNTIME_FLOAT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
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
            request->_tempObject = new String();
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (request->_tempObject) {
                String* body = static_cast<String*>(request->_tempObject);
                for (size_t i = 0; i < len; i++) {
                    *body += (char)data[i];
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

                    delete body;
                    request->_tempObject = nullptr;
                }
            }
        });
#endif
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

void ConfigManagerWeb::setSettingsPassword(const String& password) {
    settingsPassword = password;
    WEB_LOG("[Web] Settings password configured (length: %d)", password.length());
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
    WEB_LOG("[Web] Settings auth token issued (ttl=%lus)", (unsigned long)(SETTINGS_AUTH_TTL_MS / 1000UL));
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

