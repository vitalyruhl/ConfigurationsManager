// [MOCKED!] HTTP/Arduino adapters; OTA lifecycle/gate and extracted methods are production code.
#include "ota/detail/OtaUploadGate.h"
#include "ota/detail/HttpOtaRebootRequest.h"
#include "ota/detail/HttpRebootPreparation.h"
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <future>
#include <thread>
#include <iostream>
#include <string>

template <typename... Args>
void fakeLog(Args...) {}
#define OTA_LOG(...) fakeLog(__VA_ARGS__)
using String = std::string;
struct RestartTestSerial {
  template <class... Args>
  int printf(const char*, Args...) {
    return 0;
  }
};
RestartTestSerial Serial;
uint32_t clockMs = 0;
unsigned delays = 0;
uint32_t millis() {
  return clockMs;
}
void delay(unsigned) {
  ++delays;
}
struct AsyncWebServerResponse {
  void addHeader(const char*, const char*) {}
};
struct OtaUploadContext {
  uint32_t sessionId = 0;
  bool authorized = true, success = true, hasError = false;
  size_t written = 4096;
  int statusCode = 500;
  String errorReason = "test";
};
struct AsyncWebServerRequest {
  OtaUploadContext* _tempObject = nullptr;
  std::function<void()> disconnected;
  AsyncWebServerResponse response;
  unsigned sent = 0;
  AsyncWebServerResponse* beginResponse(int code, const char*, const char*) {
    assert(code == 200);
    return &response;
  }
  void send(AsyncWebServerResponse*) {
    ++sent;
  }
  void send(int, const char*, const String&) {
    ++sent;
  }
  bool hasHeader(const char*) {
    return false;
  }
  void onDisconnect(std::function<void()> f) {
    disconnected = std::move(f);
  }
};
struct {
  void abort() {}
} Update;
struct {
  unsigned polls = 0;
  void handle() {
    ++polls;
  }
} ArduinoOTA;
struct ConfigManagerOTA {
  using RebootCallback = std::function<void()>;
  using LogCallback = std::function<void(const char*)>;
  LogCallback logCallback;
  void setCallbacks(RebootCallback, LogCallback);
  static constexpr uint32_t OTA_STALE_TIMEOUT_MS = 120000;
  std::atomic<bool> otaEnabled{true}, otaInitialized{true}, otaActive{false}, disablePending{false};
  std::atomic<uint32_t> otaLastActivityMs{0}, pendingAbortSession{0};
  cm::detail::OtaLifecycleState lifecycle;
  cm::detail::OtaUploadGate uploadGate{lifecycle};
  uint32_t arduinoSessionId = 0;
  cm::detail::HttpOtaRebootRequest httpReboot;
  cm::detail::HttpRebootPreparation httpRebootPreparation;
  std::function<void()> rebootCallback;
  std::function<bool()> rebootPreparation;
  void cleanup(AsyncWebServerRequest* request) {
    delete request->_tempObject;
    request->_tempObject = nullptr;
  }
  void setActive(bool value) {
    otaActive = value;
  }
  void checkStaleUpload() {}
  void notifyActivityState() {}
  void handle();
  void sendHttpSuccess(AsyncWebServerRequest*, const OtaUploadContext*);
  void handleOTAUpload(AsyncWebServerRequest*);
};
#include "ota-http-reboot-methods.inc"

static std::function<void()> completeSuccess(ConfigManagerOTA& ota) {
  assert(ota.lifecycle.tryStart(OtaTransport::Http));
  const auto id = ota.lifecycle.activeSessionId();
  ota.lifecycle.finish(id, OtaStage::Succeeded, 0);
  AsyncWebServerRequest request;
  request._tempObject = new OtaUploadContext;
  request._tempObject->sessionId = id;
  ota.handleOTAUpload(&request);
  assert(request.sent == 1 && request.disconnected && !request._tempObject);
  return request.disconnected; // Request and success context no longer exist.
}
int main() {
  ConfigManagerOTA ota;
  unsigned reboots = 0;
  bool insideDisconnect = false;
  ota.rebootCallback = [&] { assert(!insideDisconnect);++reboots; };
  clockMs = 1000;
  auto disconnect = completeSuccess(ota);
  insideDisconnect = true;
  disconnect();
  insideDisconnect = false;
  assert(delays == 0 && reboots == 0);
  clockMs = 1400;
  disconnect();
  clockMs = 1499;
  ota.handle();
  assert(reboots == 0);
  clockMs = 1500;
  ota.handle();
  assert(reboots == 1);
  disconnect();
  clockMs = 3000;
  ota.handle();
  assert(reboots == 1);
  // A committed success is serviced before disabled/uninitialized returns or gate ownership.
  for (unsigned mode = 0; mode < 3; ++mode) {
    ConfigManagerOTA early;
    unsigned calls = 0;
    early.rebootCallback = [&] { ++calls; };
    clockMs = 4000;
    auto callback = completeSuccess(early);
    callback();
    if (mode == 0)
      early.otaEnabled = false;
    if (mode == 1)
      early.otaInitialized = false;
    if (mode == 2)
      early.disablePending = true;
    clockMs = 4500;
    early.handle();
    assert(calls == 1);
    early.handle();
    assert(calls == 1);
  }
  // Reboot polling must run before an upload mutex could exclude normal servicing.
  {
    ConfigManagerOTA locked;
    unsigned calls = 0;
    locked.rebootCallback = [&] { ++calls; };
    clockMs = 5000;
    auto callback = completeSuccess(locked);
    callback();
    std::promise<void> held, release;
    auto released = release.get_future();
    std::thread owner([&] {auto lock=locked.uploadGate.tryRequest();assert(lock.owns_lock());held.set_value();released.wait(); });
    held.get_future().wait();
    clockMs = 5500;
    locked.handle();
    assert(calls == 1);
    release.set_value();
    owner.join();
  }
  for (unsigned invalid = 0; invalid < 5; ++invalid) {
    ConfigManagerOTA rejected;
    unsigned calls = 0;
    rejected.rebootCallback = [&] { ++calls; };
    assert(rejected.lifecycle.tryStart(OtaTransport::Http));
    auto id = rejected.lifecycle.activeSessionId();
    rejected.lifecycle.finish(id, invalid == 1 ? OtaStage::Failed : OtaStage::Succeeded, 0);
    AsyncWebServerRequest request;
    request._tempObject = new OtaUploadContext;
    request._tempObject->sessionId = invalid == 2 ? id + 1 : id;
    request._tempObject->authorized = invalid != 0;
    request._tempObject->hasError = invalid == 3;
    request._tempObject->success = invalid != 4;
    rejected.handleOTAUpload(&request);
    assert(!request.disconnected && !request._tempObject);
    clockMs += 1000;
    rejected.handle();
    assert(calls == 0);
  }
  // A consumer may require terminal radio shutdown before the HTTP reboot.
  {
    ConfigManagerOTA prepared;
    unsigned calls = 0, polls = 0;
    bool ready = false;
    prepared.rebootCallback = [&] { ++calls; };
    prepared.rebootPreparation = [&] {++polls;return ready; };
    prepared.setCallbacks([&] { ++calls; }, {});
    clockMs = 8000;
    auto callback = completeSuccess(prepared);
    callback();
    clockMs = 8500;
    prepared.handle();
    assert(calls == 0 && polls == 1);
    clockMs = 9000;
    prepared.handle();
    assert(calls == 0 && polls == 2);
    ready = true;
    clockMs = 9001;
    prepared.handle();
    assert(calls == 1);
    prepared.handle();
    assert(calls == 1);
  }
  {
    ConfigManagerOTA timeout;
    unsigned calls = 0, polls = 0;
    bool ready = false;
    timeout.rebootCallback = [&] { ++calls; };
    timeout.rebootPreparation = [&] {++polls;return ready; };
    clockMs = 10000;
    auto callback = completeSuccess(timeout);
    callback();
    clockMs = 10500;
    timeout.handle();
    assert(polls == 1 && calls == 0);
    clockMs = 20499;
    callback();
    timeout.handle();
    assert(polls == 2);
    clockMs = 20500;
    timeout.handle();
    assert(timeout.httpRebootPreparation.blocked() && calls == 0);
    ready = true;
    clockMs = 20501;
    timeout.handle();
    assert(calls == 0 && polls == 2);
  }
  std::cout << "[MOCKED!] HTTP reboot runs only from servicing after500ms, once, without request lifetime dependency\n";
}
