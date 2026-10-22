#include "web/StaticGzipResponse.h"
#include "html_content.h"
#include <cassert>
#include <iostream>
#include <memory>

static void acknowledge(cm::StaticGzipResponse& response, AsyncWebServerRequest& request, AsyncClient& client, size_t count) {
  assert(count <= client.outstanding);
  client.outstanding -= count;
  response._ack(&request, count, 0);
}
static void drain(cm::StaticGzipResponse& response, AsyncWebServerRequest& request, AsyncClient& client) {
  for (size_t step = 0; step < 20000 && !response._finished(); ++step) {
    if (client.outstanding)
      acknowledge(response, request, client, std::min<size_t>(client.outstanding, 73));
    else
      response._ack(&request, 0, 0);
    assert(client.outstanding <= 512);
  }
  assert(response._finished() && client.closed);
  assert(response.totalAccepted() == response.totalAcknowledged());
  assert(client.maximumOffer <= 512 && client.maximumOutstanding <= 512);
  const auto boundary = client.accepted.find("\r\n\r\n");
  assert(boundary != std::string::npos);
  const auto header = client.accepted.substr(0, boundary);
  assert(header.find("Content-Encoding: gzip") != std::string::npos);
  assert(header.find("Connection: close") != std::string::npos);
  assert(header.find("Content-Type: text/html") != std::string::npos);
  assert(header.find("Content-Length: " + std::to_string(WEB_HTML_GZ_LEN)) != std::string::npos);
  assert(client.accepted.substr(boundary + 4) == std::string(reinterpret_cast<const char*>(WEB_HTML_GZ), WEB_HTML_GZ_LEN));
}
int main() {
  {
    AsyncClient client;
    AsyncWebServerRequest request(&client);
    client.acceptLimits = {0, 7, 0, 9, 10000, 0, 11, 0, 23};
    cm::StaticGzipResponse response(WEB_HTML_GZ, WEB_HTML_GZ_LEN);
    response._respond(&request);
    assert(client.accepted.empty() && response.bodyAccepted() == 0);
    response._ack(&request, 0, 0);
    assert(client.accepted == "HTTP/1.");
    const auto unchanged = client.accepted;
    response._ack(&request, 0, 0);
    assert(client.accepted == unchanged);
    drain(response, request, client);
  }
  {
    AsyncClient client;
    AsyncWebServerRequest request(&client);
    client.flushResults = {false, false, true};
    cm::StaticGzipResponse response(WEB_HTML_GZ, WEB_HTML_GZ_LEN);
    response._respond(&request);
    assert(!client.accepted.empty());
    const auto queued = client.accepted;
    response._ack(&request, 0, 0);
    assert(client.accepted == queued); // Failed flush must not re-add accepted headers.
    response._ack(&request, 0, 0);
    assert(client.accepted.find("HTTP/1.1", 1) == std::string::npos);
    client.flushResults = {false, false, true}; // Same failure after body bytes are accepted.
    acknowledge(response, request, client, std::min<size_t>(client.outstanding, 37));
    const auto bodyQueued = client.accepted;
    response._ack(&request, 0, 0);
    assert(client.accepted == bodyQueued);
    drain(response, request, client);
  }
  {
    AsyncClient client;
    AsyncWebServerRequest request(&client);
    client.available = 0;
    cm::StaticGzipResponse response(WEB_HTML_GZ, WEB_HTML_GZ_LEN);
    response._respond(&request);
    assert(client.accepted.empty());
    client.available = 16384;
    for (int i = 0; i < 10; ++i)
      response._ack(&request, 0, 0);
    assert(client.outstanding == 512 && !response._finished());
    drain(response, request, client);
  }
  {
    AsyncClient client;
    AsyncWebServerRequest request(&client);
    auto response = std::make_unique<cm::StaticGzipResponse>(WEB_HTML_GZ, WEB_HTML_GZ_LEN);
    response->_respond(&request);
    while (response->bodyAccepted() < WEB_HTML_GZ_LEN)
      acknowledge(*response, request, client, client.outstanding);
    assert(!response->_finished() && !client.closed);
    const size_t remaining = client.outstanding;
    if (remaining > 1)
      acknowledge(*response, request, client, remaining - 1);
    assert(!response->_finished() && !client.closed && client.outstanding == 1);
    client.onClose = [&]() { response.reset(); }; // Synchronous disconnect may destroy the response.
    auto* raw = response.get();
    client.outstanding = 0;
    raw->_ack(&request, 1, 0);
    assert(!response && client.closed);
  }
  std::cout << "Static gzip response passed: exact " << WEB_HTML_GZ_LEN << " body bytes; add0/short, flush failure, poll, partial/final ACK; max outstanding512\n";
}
