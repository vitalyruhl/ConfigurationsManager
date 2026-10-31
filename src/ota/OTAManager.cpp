#include "OTAManager.h"
#include "../ConfigManager.h"

// Logging support
#define OTA_LOG(...) CM_LOG("[OTA] " __VA_ARGS__)

ConfigManagerOTA::ConfigManagerOTA()
    : otaEnabled(false), otaInitialized(false), otaActive(false), webRoutesConfigured(false), otaLastActivityMs(0), configManager(nullptr) {
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

void ConfigManagerOTA::markActivity() {
  otaLastActivityMs = millis();
}

void ConfigManagerOTA::notifyActivityState() {
  if (configManager) {
    configManager->notifyOTAStatus();
    configManager->pushRuntimeSnapshot();
  }
}

void ConfigManagerOTA::setActive(bool active) {
  if (active) {
    markActivity();
  } else {
    otaLastActivityMs = 0;
  }

  if (otaActive == active) {
    return;
  }

  otaActive = active;
  notifyActivityState();
}

void ConfigManagerOTA::checkStaleUpload() {
  if (!otaActive) {
    return;
  }

  const uint32_t now = millis();
  if (!lifecycle.expire(lifecycle.activeSessionId(), now, otaLastActivityMs, OTA_STALE_TIMEOUT_MS)) {
    return;
  }

  OTA_LOG("[W] Upload stale, clearing OTA active state");
  if (Update.isRunning()) {
    Update.abort();
  }
  setActive(false);
}

void ConfigManagerOTA::setup(const String& hostname, const String& password) {
  otaHostname = hostname;
  otaPassword = password;

  if (!otaInitialized) {
    ArduinoOTA.setHostname(otaHostname.c_str());

    if (!otaPassword.isEmpty()) {
      ArduinoOTA.setPassword(otaPassword.c_str());
      OTA_LOG("Password protection enabled");
    }

    ArduinoOTA.onStart([this]() {
      if (configManager)
        configManager->noteSynchronousOtaStart();
      if (!lifecycle.tryStart(OtaTransport::Arduino)) {
        // Update.begin already rejects an active HTTP upload in Arduino core 2.0.17.
        Update.abort();
        return;
      }
      arduinoSessionId = lifecycle.activeSessionId();
      setActive(true);
      String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
      OTA_LOG("Start updating %s", type.c_str());
    });

    ArduinoOTA.onEnd([this]() {
      if (configManager)
        configManager->noteLoopProgress();
      lifecycle.finish(arduinoSessionId, OtaStage::Succeeded, 0);
      arduinoSessionId = 0;
      setActive(false);
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
      if (configManager)
        configManager->noteLoopProgress();
      if (!otaEnabled && arduinoSessionId) {
        Update.abort();
        lifecycle.finish(arduinoSessionId, OtaStage::Aborted, 0);
        arduinoSessionId = 0;
        setActive(false);
        return;
      }
      markActivity();
      static unsigned int lastPercent = 0;
      unsigned int percent = total ? static_cast<unsigned int>((uint64_t(progress) * 100) / total) : 0;
      if (percent != lastPercent && percent % 10 == 0) {
        OTA_LOG("Progress: %u%%", percent);
        lastPercent = percent;
      }
    });

    ArduinoOTA.onError([this](ota_error_t error) {
      if (configManager)
        configManager->noteLoopProgress();
      // Authentication/begin errors before onStart do not own the HTTP session.
      if (arduinoSessionId) {
        lifecycle.finish(arduinoSessionId, OtaStage::Failed, error);
        arduinoSessionId = 0;
        setActive(false);
      }
      const char* errorStr = "Unknown";
      switch (error) {
        case OTA_AUTH_ERROR:
          errorStr = "Auth Failed";
          break;
        case OTA_BEGIN_ERROR:
          errorStr = "Begin Failed";
          break;
        case OTA_CONNECT_ERROR:
          errorStr = "Connect Failed";
          break;
        case OTA_RECEIVE_ERROR:
          errorStr = "Receive Failed";
          break;
        case OTA_END_ERROR:
          errorStr = "End Failed";
          break;
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
  auto lock = uploadGate.tryRequest();
  if (!lock.owns_lock()) {
    if (!enabled)
      disablePending = true;
    return;
  }
  if (!enabled) {
    if (lifecycle.activeSessionId()) {
      Update.abort();
      lifecycle.finish(lifecycle.activeSessionId(), OtaStage::Aborted, 0);
      arduinoSessionId = 0;
    }
    setActive(false);
  }
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
  // HTTP only: preparation and restart remain outside upload locks and callback tasks.
  if (httpReboot.takeIfDue(millis())) {
    httpRebootPreparation.begin(millis());
#if CM_TEST_RESTART_PHASES
    Serial.printf("[I] [MOCKED!] RESTART_PHASE HTTP_PREPARE_BEGIN ms=%lu\n", static_cast<unsigned long>(millis()));
#endif
  }
  const auto prepared = httpRebootPreparation.poll(millis(), [this]() {
    return !rebootPreparation || rebootPreparation();
  });
  if (prepared == cm::detail::HttpRebootPreparation::Result::Waiting)
    return;
  if (prepared == cm::detail::HttpRebootPreparation::Result::TimedOut) {
    OTA_LOG("[E] HTTP reboot preparation timed out; automatic restart blocked");
#if CM_TEST_RESTART_PHASES
    Serial.printf("[I] [MOCKED!] RESTART_PHASE HTTP_PREPARE_TIMEOUT ms=%lu\n", static_cast<unsigned long>(millis()));
#endif
    return;
  }
  if (prepared == cm::detail::HttpRebootPreparation::Result::Ready) {
#if CM_TEST_RESTART_PHASES
    Serial.printf("[I] [MOCKED!] RESTART_PHASE ESP_RESTART_BEGIN ms=%lu\n", static_cast<unsigned long>(millis()));
#endif
    if (rebootCallback)
      rebootCallback();
    return;
  }
  const bool stale = otaActive && uint32_t(millis() - otaLastActivityMs.load()) >= OTA_STALE_TIMEOUT_MS;
  const auto active = lifecycle.activeSessionId();
  auto pendingAbort = pendingAbortSession.load();
  if (pendingAbort && pendingAbort != active)
    pendingAbortSession.compare_exchange_strong(pendingAbort, 0);
  const bool maintenance = (disablePending.load() && !otaEnabled) ||
                           (pendingAbort && pendingAbort == active) || stale;
  auto lock = uploadGate.tryService(maintenance);
  if (!lock.owns_lock())
    return;
  if (disablePending.exchange(false) && !otaEnabled) {
    auto id = lifecycle.activeSessionId();
    if (id) {
      Update.abort();
      lifecycle.finish(id, OtaStage::Aborted, 0);
      arduinoSessionId = 0;
      setActive(false);
    }
  }
  const auto pending = pendingAbortSession.exchange(0);
  if (pending && pending == lifecycle.activeSessionId()) {
    Update.abort();
    lifecycle.finish(pending, OtaStage::Aborted, 0);
    setActive(false);
  }
  if (!otaEnabled || !otaInitialized) {
    checkStaleUpload();
    return;
  }

  checkStaleUpload();
  // Normal Arduino polling never owns Update while an HTTP owner is admitted.
  if (lifecycle.isActive(OtaTransport::Http))
    return;
  // ArduinoOTA itself handles the accepted transfer synchronously.
  ArduinoOTA.handle();
  checkStaleUpload();
}

bool ConfigManagerOTA::isActive() const {
  return otaInitialized && otaEnabled && otaActive;
}

void ConfigManagerOTA::setupWebRoutes(AsyncWebServer* webServer) {
  if (!webServer)
    return;
  if (webRoutesConfigured) {
    OTA_LOG("[D] Web routes already configured");
    return;
  }

  // OTA upload endpoint
  webServer->on("/ota_update", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!otaEnabled) {
      request->send(403, "application/json", "{\"status\":\"error\",\"reason\":\"ota_disabled\"}");
      return;
    }
    if (otaActive) {
      notifyActivityState();
    }
    // Treat GET as a lightweight probe endpoint for clients/UI.
    request->send(200, "application/json", String("{\"status\":\"ok\",\"probe\":true,\"otaActive\":") + (otaActive ? "true" : "false") + "}");
  });

  webServer->on("/ota_update", HTTP_POST, [this](AsyncWebServerRequest* request) { handleOTAUpload(request); }, [this](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) { handleOTAUploadData(request, filename, index, data, len, final); });

  webRoutesConfigured = true;
  OTA_LOG("[I] Web routes configured");
}

void ConfigManagerOTA::handleOTAUpload(AsyncWebServerRequest* request) {
  auto* completed = static_cast<OtaUploadContext*>(request->_tempObject);
  if (completed && completed->authorized && completed->success && !completed->hasError &&
      uploadGate.completeHttp(completed->sessionId, [this, request, completed] { sendHttpSuccess(request, completed); }))
    return;

  auto lock = uploadGate.tryRequest();
  if (!lock.owns_lock()) {
    request->send(409, "application/json", "{\"status\":\"error\",\"reason\":\"ota_busy\"}");
    cleanup(request);
    return;
  }
  // Handle probe requests first (they have no body, so no context is created)
  if (request->hasHeader("X-OTA-PROBE")) {
    if (!otaEnabled) {
      request->send(403, "application/json", "{\"status\":\"error\",\"reason\":\"ota_disabled\"}");
    } else {
      if (otaActive) {
        notifyActivityState();
      }
      request->send(200, "application/json", String("{\"status\":\"ok\",\"probe\":true,\"otaActive\":") + (otaActive ? "true" : "false") + "}");
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
    request->send(ctx->statusCode, "application/json", String("{\"status\":\"error\",\"reason\":\"") + ctx->errorReason + "\"}");
    cleanup(request);
    return;
  }

  if (!ctx->success) {
    OTA_LOG("Upload incomplete");
    request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"incomplete\"}");
    cleanup(request);
    return;
  }

  // A success context without the retained accepted owner is never acknowledged.
  request->send(409, "application/json", "{\"status\":\"error\",\"reason\":\"stale_session\"}");
  cleanup(request);
}

void ConfigManagerOTA::sendHttpSuccess(AsyncWebServerRequest* request, const OtaUploadContext* ctx) {
  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"status\":\"ok\",\"action\":\"reboot\"}");
  response->addHeader("Connection", "close");

  request->onDisconnect([this]() {
    httpReboot.request(millis());
  });

  // Cppcheck rationale: The value is consumed by a compile-time logging macro in enabled builds.
  // cppcheck-suppress unreadVariable
  size_t uploaded = ctx->written;
  request->send(response);
  OTA_LOG("HTTP upload success (%lu bytes)", static_cast<unsigned long>(uploaded));
  cleanup(request);
}

void ConfigManagerOTA::handleOTAUploadData(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
  auto lock = uploadGate.tryRequest();
  OtaUploadContext* ctx = static_cast<OtaUploadContext*>(request->_tempObject);

  if (index == 0) {
    if (ctx) {
      ctx->hasError = true;
      ctx->statusCode = 400;
      ctx->errorReason = "duplicate_start";
      if (ctx->sessionId)
        pendingAbortSession = ctx->sessionId;
      return;
    }
    ctx = new OtaUploadContext();
    request->_tempObject = ctx;

    request->onDisconnect([this, request]() { cleanup(request); });
    if (!lock.owns_lock()) {
      ctx->hasError = true;
      ctx->statusCode = 409;
      ctx->errorReason = "ota_busy";
      return;
    }

    if (!otaEnabled) {
      ctx->hasError = true;
      ctx->statusCode = 403;
      ctx->errorReason = "ota_disabled";
      return;
    }

    if (!otaPassword.isEmpty()) {
      const AsyncWebHeader* hdr = request->getHeader("X-OTA-PASSWORD");
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

    if (!lifecycle.tryStart(OtaTransport::Http)) {
      ctx->hasError = true;
      ctx->statusCode = 409;
      ctx->errorReason = "ota_busy";
      return;
    }
    ctx->sessionId = lifecycle.activeSessionId();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      ctx->hasError = true;
      ctx->statusCode = 500;
      ctx->errorReason = "begin_failed";
      lifecycle.finish(ctx->sessionId, OtaStage::Failed, Update.getError());
      setActive(false);
      Update.printError(Serial);
      return;
    }

    ctx->began = true;
    setActive(true);
    OTA_LOG("HTTP upload start: %s (%lu bytes)", filename.c_str(), static_cast<unsigned long>(expected));
  }

  if (!lock.owns_lock() && ctx) {
    ctx->hasError = true;
    ctx->statusCode = 409;
    ctx->errorReason = "ota_busy";
    if (ctx->sessionId)
      pendingAbortSession = ctx->sessionId;
    return;
  }
  if (!ctx || ctx->hasError || !ctx->authorized) {
    return;
  }

  if (ctx->sessionId != lifecycle.activeSessionId()) {
    ctx->hasError = true;
    ctx->statusCode = 409;
    ctx->errorReason = "stale_session";
    return;
  }

  if (ctx->probe) {
    return;
  }

  if (len) {
    markActivity();
    if (Update.write(data, len) != len) {
      ctx->hasError = true;
      ctx->statusCode = 500;
      ctx->errorReason = "write_failed";
      lifecycle.finish(ctx->sessionId, OtaStage::Failed, Update.getError());
      Update.abort();
      setActive(false);
      Update.printError(Serial);
      return;
    }
    ctx->written += len;
  }

  if (final) {
    if (Update.end(true)) {
      ctx->success = true;
      lifecycle.finish(ctx->sessionId, OtaStage::Succeeded, 0);
      setActive(false);
      // Refresh runtime sketch metrics cache so UI shows updated values without reboot
      if (configManager) {
        configManager->getRuntimeManager().refreshSketchInfoCache();
      }
    } else {
      ctx->hasError = true;
      ctx->statusCode = 500;
      ctx->errorReason = "end_failed";
      lifecycle.finish(ctx->sessionId, OtaStage::Failed, Update.getError());
      setActive(false);
      Update.printError(Serial);
      Update.abort();
    }
  }
}

String ConfigManagerOTA::getStatus() const {
  if (!otaEnabled)
    return "disabled";
  if (!otaInitialized)
    return "not_initialized";
  if (isActive())
    return "active";
  return "ready";
}

void ConfigManagerOTA::cleanup(AsyncWebServerRequest* request) {
#if CM_ENABLE_OTA
  if (!request) {
    return;
  }

  auto lock = uploadGate.tryRequest();
  auto* ctx = static_cast<OtaUploadContext*>(request->_tempObject);
  if (!ctx) {
    return;
  }

  if (!lock.owns_lock()) {
    if (ctx->sessionId && !ctx->success)
      pendingAbortSession = ctx->sessionId;
  } else if (ctx->sessionId && ctx->sessionId == lifecycle.activeSessionId()) {
    OTA_LOG("Aborting incomplete update");
    Update.abort();
    lifecycle.finish(ctx->sessionId, OtaStage::Aborted, 0);
    setActive(false);
  }

  delete ctx;
  request->_tempObject = nullptr;
#else
  (void)request;
#endif
}

#endif // CM_ENABLE_OTA
