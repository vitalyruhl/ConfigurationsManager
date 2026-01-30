#include "OTAManager.h"
#include "../ConfigManager.h"

// Logging support
#define OTA_LOG(...) CM_LOG("[OTA] " __VA_ARGS__)

ConfigManagerOTA::ConfigManagerOTA()
    : otaEnabled(false)
    , otaInitialized(false)
    , configManager(nullptr)
{
}

ConfigManagerOTA::~ConfigManagerOTA() {
#if CM_ENABLE_OTA
    if (otaInitialized) {
        ArduinoOTA.end();
    }
#endif
}

void ConfigManagerOTA::begin(ConfigManagerClass* cm) {
    configManager = cm;
    OTA_LOG("OTA manager initialized");
}

void ConfigManagerOTA::setCallbacks(RebootCallback reboot, LogCallback logger) {
    rebootCallback = reboot;
    logCallback = logger;
}

#if CM_ENABLE_OTA

void ConfigManagerOTA::setup(const String& hostname, const String& password) {
    otaHostname = hostname;
    otaPassword = password;

    if (WiFi.status() != WL_CONNECTED) {
        OTA_LOG("WiFi not connected, skipping OTA setup");
        return;
    }

    if (!otaInitialized) {
        ArduinoOTA.setHostname(otaHostname.c_str());

        if (!otaPassword.isEmpty()) {
            ArduinoOTA.setPassword(otaPassword.c_str());
            OTA_LOG("Password protection enabled");
        }

        ArduinoOTA.onStart([this]() {
            String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
            OTA_LOG("Start updating %s", type.c_str());
        });

        ArduinoOTA.onEnd([this]() {
            OTA_LOG("Update complete");
            // Some ESP32/LwIP combinations can hit a TCP assert shortly after OTA completes.
            // Reboot immediately after a successful update to leave the network stack in a clean state.
            OTA_LOG("Rebooting after OTA...");
            if (rebootCallback) {
                rebootCallback();
            } else {
                ESP.restart();
            }
        });

        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            static unsigned int lastPercent = 0;
            unsigned int percent = (progress / (total / 100));
            if (percent != lastPercent && percent % 10 == 0) {
                OTA_LOG("Progress: %u%%", percent);
                lastPercent = percent;
            }
        });

        ArduinoOTA.onError([this](ota_error_t error) {
            const char* errorStr = "Unknown";
            switch (error) {
                case OTA_AUTH_ERROR: errorStr = "Auth Failed"; break;
                case OTA_BEGIN_ERROR: errorStr = "Begin Failed"; break;
                case OTA_CONNECT_ERROR: errorStr = "Connect Failed"; break;
                case OTA_RECEIVE_ERROR: errorStr = "Receive Failed"; break;
                case OTA_END_ERROR: errorStr = "End Failed"; break;
            }
            OTA_LOG("Error[%u]: %s", error, errorStr);
        });

        ArduinoOTA.begin();
        otaInitialized = true;
        OTA_LOG("Arduino OTA started on %s", otaHostname.c_str());
    }

    otaEnabled = true;
}

void ConfigManagerOTA::enable(bool enabled) {
    otaEnabled = enabled;
    OTA_LOG("%s", enabled ? "Enabled" : "Disabled");
}

void ConfigManagerOTA::setPassword(const String& password) {
    otaPassword = password;
    if (otaInitialized) {
        ArduinoOTA.setPassword(password.c_str());
    }
}

void ConfigManagerOTA::setHostname(const String& hostname) {
    otaHostname = hostname;
    if (otaInitialized) {
        ArduinoOTA.setHostname(hostname.c_str());
    }
}

void ConfigManagerOTA::handle() {
    if (!otaEnabled || !otaInitialized) {
        return;
    }

    // Non-blocking OTA handle
    ArduinoOTA.handle();
}

bool ConfigManagerOTA::isActive() const {
    // Check if OTA update is in progress
    return otaInitialized && otaEnabled; // Could be enhanced with actual progress check
}

void ConfigManagerOTA::setupWebRoutes(AsyncWebServer* server) {
    if (!server) return;

    // OTA upload endpoint
    server->on("/ota_update", HTTP_GET,
        [this](AsyncWebServerRequest* request) {
            if (!otaEnabled) {
                request->send(403, "application/json", "{\"status\":\"error\",\"reason\":\"ota_disabled\"}");
                return;
            }
            // Treat GET as a lightweight probe endpoint for clients/UI.
            request->send(200, "application/json", "{\"status\":\"ok\",\"probe\":true}");
        });

    server->on("/ota_update", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            handleOTAUpload(request);
        },
        [this](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            handleOTAUploadData(request, filename, index, data, len, final);
        }
    );

    OTA_LOG("Web routes configured");
}

void ConfigManagerOTA::handleOTAUpload(AsyncWebServerRequest* request) {
    // Handle probe requests first (they have no body, so no context is created)
    if (request->hasHeader("X-OTA-PROBE")) {
        if (!otaEnabled) {
            request->send(403, "application/json", "{\"status\":\"error\",\"reason\":\"ota_disabled\"}");
        } else {
            request->send(200, "application/json", "{\"status\":\"ok\",\"probe\":true}");
        }
        return;
    }

    OtaUploadContext* ctx = static_cast<OtaUploadContext*>(request->_tempObject);

    if (!ctx) {
        request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"no_context\"}");
        cleanup(request);
        return;
    }

    if (ctx->hasError) {
        OTA_LOG("Upload failed: %s", ctx->errorReason.c_str());
        request->send(ctx->statusCode, "application/json",
            String("{\"status\":\"error\",\"reason\":\"") + ctx->errorReason + "\"}");
        cleanup(request);
        return;
    }

    if (!ctx->success) {
        OTA_LOG("Upload incomplete");
        request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"incomplete\"}");
        cleanup(request);
        return;
    }

    AsyncWebServerResponse* response = request->beginResponse(200, "application/json",
        "{\"status\":\"ok\",\"action\":\"reboot\"}");
    response->addHeader("Connection", "close");

    request->onDisconnect([this]() {
        OTA_LOG("HTTP client disconnected, rebooting...");
        delay(500);
        if (rebootCallback) {
            rebootCallback();
        }
    });

    size_t uploaded = ctx->written;
    request->send(response);
    OTA_LOG("HTTP upload success (%lu bytes)", static_cast<unsigned long>(uploaded));
    cleanup(request);
}

void ConfigManagerOTA::handleOTAUploadData(AsyncWebServerRequest* request, String filename,
                                          size_t index, uint8_t* data, size_t len, bool final) {
    OtaUploadContext* ctx = static_cast<OtaUploadContext*>(request->_tempObject);

    if (index == 0) {
        ctx = new OtaUploadContext();
        request->_tempObject = ctx;

        if (Update.isRunning()) {
            OTA_LOG("Existing update in progress, aborting prior session");
            Update.abort();
        }

        if (!otaEnabled) {
            ctx->hasError = true;
            ctx->statusCode = 403;
            ctx->errorReason = "ota_disabled";
            return;
        }

        if (!otaPassword.isEmpty()) {
            AsyncWebHeader* hdr = request->getHeader("X-OTA-PASSWORD");
            if (!hdr) {
                ctx->hasError = true;
                ctx->statusCode = 401;
                ctx->errorReason = "missing_password";
                return;
            }
            if (hdr->value() != otaPassword) {
                ctx->hasError = true;
                ctx->statusCode = 401;
                ctx->errorReason = "unauthorized";
                return;
            }
        }
        ctx->authorized = true;

        size_t expected = request->contentLength();
        if (expected == 0) {
            ctx->hasError = true;
            ctx->statusCode = 400;
            ctx->errorReason = "empty_upload";
            return;
        }

        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            ctx->hasError = true;
            ctx->statusCode = 500;
            ctx->errorReason = "begin_failed";
            Update.printError(Serial);
            return;
        }

        ctx->began = true;
        OTA_LOG("HTTP upload start: %s (%lu bytes)", filename.c_str(), static_cast<unsigned long>(expected));
    }

    if (!ctx || ctx->hasError || !ctx->authorized) {
        return;
    }

    if (ctx->probe) {
        return;
    }

    if (len) {
        if (Update.write(data, len) != len) {
            ctx->hasError = true;
            ctx->statusCode = 500;
            ctx->errorReason = "write_failed";
            Update.printError(Serial);
            return;
        }
        ctx->written += len;
    }

    if (final) {
        if (Update.end(true)) {
            ctx->success = true;
            // Refresh runtime sketch metrics cache so UI shows updated values without reboot
            if (configManager) {
                configManager->getRuntimeManager().refreshSketchInfoCache();
            }
        } else {
            ctx->hasError = true;
            ctx->statusCode = 500;
            ctx->errorReason = "end_failed";
            Update.printError(Serial);
            Update.abort();
        }
    }
}

String ConfigManagerOTA::getStatus() const {
    if (!otaEnabled) return "disabled";
    if (!otaInitialized) return "not_initialized";
    if (isActive()) return "active";
    return "ready";
}

void ConfigManagerOTA::cleanup(AsyncWebServerRequest* request) {
#if CM_ENABLE_OTA
    if (!request) {
        return;
    }

    auto* ctx = static_cast<OtaUploadContext*>(request->_tempObject);
    if (!ctx) {
        return;
    }

    if (ctx->began && !ctx->success) {
        OTA_LOG("Aborting incomplete update");
        Update.abort();
    }

    delete ctx;
    request->_tempObject = nullptr;
#else
    (void)request;
#endif
}

#endif // CM_ENABLE_OTA
