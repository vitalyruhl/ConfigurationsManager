#include "web/BoundedResponse.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <string>

#if __has_include("web/ThemeCssSnapshot.h")
#include "web/ThemeCssSnapshot.h"

// [MOCKED!] Arduino String allocation/failure shell; transport is production code.
struct Output {
  std::string text = "old";
  bool failReserve = false;
  size_t failAfter = std::numeric_limits<size_t>::max();
  unsigned reserveCalls = 0;
  void remove(unsigned) { text.clear(); }
  bool reserve(unsigned length) {
    ++reserveCalls;
    if (failReserve) return false;
    text.reserve(length);
    return true;
  }
  bool concat(char byte) {
    if (text.size() >= failAfter) return false;
    text.push_back(byte);
    return true;
  }
  size_t length() const { return text.length(); }
};

static void verifyResponse(cm::BoundedResponse& response, const std::string& expected,
                           const char* borrowed = nullptr) {
  AsyncClient client;
  AsyncWebServerRequest request(&client);
  response._respond(&request);
  for (unsigned i = 0; !client.closed && i < 10000; ++i) {
    const size_t ack = client.outstanding;
    client.outstanding -= ack;
    response._ack(&request, ack, 0);
  }
  assert(client.closed);
  const size_t split = client.accepted.find("\r\n\r\n");
  assert(split != std::string::npos);
  assert(client.accepted.substr(split + 4) == expected);
  assert(client.accepted.find("Content-Type: text/css") != std::string::npos);
  assert(client.accepted.find("Content-Length: " + std::to_string(expected.size())) != std::string::npos);
  if (borrowed) {
    assert(std::find(client.offeredPointers.begin(), client.offeredPointers.end(), borrowed) !=
           client.offeredPointers.end());
  }
}

int main() {
  // Built-in CSS is immutable static storage and must not require a body copy.
  static const char builtin[] = "{}";
  cm::BoundedResponse borrowed(reinterpret_cast<const uint8_t*>(builtin), 2, "text/css");
  verifyResponse(borrowed, "{}", builtin);

  Output snapshot;
  const std::string expected = std::string("body{color:red}") + '\0' + "/* exact bytes */";
  {
    std::string appCss = expected;
    assert(cm::snapshotThemeCss(appCss.data(), appCss.size(), snapshot));
    assert(snapshot.reserveCalls == 1);
    appCss.assign(appCss.size(), 'x');
    assert(snapshot.text == expected);
  }
  cm::BoundedResponse owned(std::move(snapshot.text), "text/css");
  verifyResponse(owned, expected);

  Output empty;
  assert(cm::snapshotThemeCss(nullptr, 0, empty));
  assert(empty.text.empty() && empty.reserveCalls == 0);
  cm::BoundedResponse noTheme(reinterpret_cast<const uint8_t*>(""), 0, "text/css");
  verifyResponse(noTheme, "");

  Output reserveFailure;
  reserveFailure.failReserve = true;
  assert(!cm::snapshotThemeCss(expected.data(), expected.size(), reserveFailure));
  assert(reserveFailure.text.empty());
  Output writeFailure;
  writeFailure.failAfter = 5;
  assert(!cm::snapshotThemeCss(expected.data(), expected.size(), writeFailure));
  assert(writeFailure.text.empty());
  Output invalid;
  assert(!cm::snapshotThemeCss(nullptr, 1, invalid));
  assert(invalid.text.empty());
  assert(!cm::snapshotThemeCss("x", std::numeric_limits<size_t>::max(), invalid));
  assert(invalid.text.empty());
  std::cout << "Theme CSS snapshots passed: borrowed builtin, owned custom exact bytes, empty and allocation failures.\n";
}
#else
int main() { std::cerr << "Missing production CSS snapshot helper.\n"; return 1; }
#endif
