// [MOCKED!] Allocation-sensitive String and transport shell; extracted production bodies are unchanged.
#include <cassert>
#include <functional>
#include <iostream>
#include <new>
#include <mutex>
#include <string>
#include <utility>
struct String {
  std::string value;
  static bool failCopies;
  static unsigned copies;
  String() = default;
  String(const char* p)
      : value(p ? p : "") {}
  String(const String& other) {
    ++copies;
    if (!failCopies)
      value = other.value;
  }
  String(String&&) noexcept = default;
  String& operator=(String&&) noexcept = default;
  size_t length() const {
    return value.size();
  }
  const char* c_str() const {
    return value.c_str();
  }
};
bool String::failCopies = false;
unsigned String::copies = 0;
struct JsonObject {};
struct JsonDocument {
  static bool overflow;
  template <class T>
  T to() {
    return T{};
  }
  bool overflowed() const {
    return overflow;
  }
};
bool JsonDocument::overflow = false;
static size_t serializationLimit = 2944;
[[maybe_unused]] static size_t measureJson(const JsonDocument&) {
  return 2944;
}
static size_t serializeJson(const JsonDocument&, String& out) {
  out.value.assign(serializationLimit, 'x');
  return 2944;
}
namespace cm {
struct RuntimeSelection {
  static constexpr size_t maxPayloadBytes = 2048;
  static RuntimeSelection everything() {
    return {};
  }
};
static bool parseRuntimeSelection(const char*, RuntimeSelection&) {
  return true;
}
} // namespace cm
class ConfigManagerRuntime {
  std::recursive_mutex runtimeGenerationMutex;

public:
#include "runtime-generation.inc"
  void fillRuntimeSnapshot(JsonObject, const cm::RuntimeSelection&) {}
  String runtimeValuesToJSON(const cm::RuntimeSelection&);
};
#include "runtime-serialization.inc"
struct AsyncWebServerResponse {
  virtual ~AsyncWebServerResponse() = default;
  void addHeader(const char*, const char*) {}
};
namespace cm {
struct BoundedResponse : AsyncWebServerResponse {
  static unsigned constructions;
  explicit BoundedResponse(String&&, const char*) {
    ++constructions;
  }
};
unsigned BoundedResponse::constructions = 0;
} // namespace cm
struct Param {
  String text;
  const String& value() const {
    return text;
  }
};
struct AsyncWebServerRequest {
  unsigned status = 0;
  bool hasParam(const char*) const {
    return false;
  }
  Param* getParam(const char*) {
    static Param p;
    return &p;
  }
  void send(unsigned code, const char*, const char*) {
    status = code;
  }
  void send(AsyncWebServerResponse* response) {
    status = 200;
    delete response;
  }
};
static constexpr unsigned HTTP_GET = 0;
struct Server {
  std::function<void(AsyncWebServerRequest*)> route;
  void on(const char*, unsigned, std::function<void(AsyncWebServerRequest*)> cb) {
    route = std::move(cb);
  }
};
#define WEB_LOG(...) \
  do {               \
  } while (false)
struct ConfigManagerWeb {
  Server transport;
  Server* server = &transport;
  std::function<String()> runtimeJsonProvider;
  std::function<String(const cm::RuntimeSelection&)> selectedRuntimeJsonProvider;
  void enableCORS(AsyncWebServerResponse*) {}
  void install() {
#include "runtime-route.inc"
  }
};
int main(int argc, char**) {
  ConfigManagerRuntime runtime;
  if (argc == 1) {
    String::failCopies = true;
    auto json = runtime.runtimeValuesToJSON({});
    assert(json.length() == 2944 && String::copies == 0);
    String::failCopies = false;
    JsonDocument::overflow = true;
    assert(runtime.runtimeValuesToJSON({}).length() == 0);
    JsonDocument::overflow = false;
    serializationLimit = 2890; // Writer's reported count alone can hide a failed final concat.
    assert(runtime.runtimeValuesToJSON({}).length() == 0);
    serializationLimit = 0;
    assert(runtime.runtimeValuesToJSON({}).length() == 0);
    std::cout << "Runtime serialization: no output copy; document/partial/empty failures rejected PASS\n";
  } else {
    ConfigManagerWeb web;
    web.runtimeJsonProvider = [] { return String(); };
    web.install();
    AsyncWebServerRequest failed;
    web.transport.route(&failed);
    assert(failed.status == 503 && cm::BoundedResponse::constructions == 0);
    web.runtimeJsonProvider = [] { return String("{}"); };
    AsyncWebServerRequest valid;
    web.transport.route(&valid);
    assert(valid.status == 200 && cm::BoundedResponse::constructions == 1);
    std::cout << "Runtime route: empty provider 503 without body response; valid provider 200 PASS\n";
  }
}
