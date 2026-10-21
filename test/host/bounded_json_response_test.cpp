#include "web/BoundedResponse.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>

static void deliverAck(cm::BoundedResponse* response, AsyncWebServerRequest& request,
                       AsyncClient& client, size_t count) {
  const size_t acknowledged = client.acknowledge(count);
  // Model one millisecond of callback processing before subsequent enqueue.
  client.advance(1);
  if (!client.closed) response->_ack(&request, acknowledged, 0);
}
static void poll(cm::BoundedResponse* response, AsyncWebServerRequest& request, AsyncClient& client) {
  // Match the pinned HTTP request's canSend gate for synthetic poll callbacks.
  if (!client.closed && client.canSend() && !response->_finished()) response->_ack(&request, 0, 0);
}
static void drain(cm::BoundedResponse* response, AsyncWebServerRequest& request, AsyncClient& client,
                  size_t limit = 1024, size_t ackSize = 97) {
  for (size_t step=0;step<10000 && !client.closed;++step) {
    deliverAck(response, request, client, ackSize);
    assert(client.outstanding <= limit);
  }
  assert(client.closed);
  assert(client.maximumOutstanding <= limit && client.maximumOffer <= limit);
}
static void verifyJson(const AsyncClient& client, const std::string& expected) {
  const auto boundary=client.accepted.find("\r\n\r\n");
  assert(boundary!=std::string::npos);
  const auto headers=client.accepted.substr(0,boundary);
  assert(headers.find("Content-Type: application/json")!=std::string::npos);
  assert(headers.find("Content-Encoding:")==std::string::npos);
  assert(headers.find("Content-Length: "+std::to_string(expected.size()))!=std::string::npos);
  assert(client.accepted.substr(boundary+4)==expected);
}
static void mssWindow() {
  // [MOCKED!] The measured UI payload needs <=23 two-MSS output steps, not 122.
  const std::string expected = "\"" + std::string(61596, 'x') + "\"";
  AsyncClient client; client.mss = 1436;
  AsyncWebServerRequest request(&client);
  cm::BoundedResponse response(String(expected), "application/json");
  response._respond(&request);
  drain(&response, request, client, 2872, 2872);
  verifyJson(client, expected);
  assert(client.maximumOffer == 2872);
  assert(client.sends <= 23);
}
static void windowBounds() {
  const std::string expected = "\"" + std::string(6000, 'x') + "\"";
  for (const auto& sample : {std::pair<uint16_t,size_t>{0,512}, {256,512}, {9000,2920}}) {
    AsyncClient client; client.mss = sample.first;
    AsyncWebServerRequest request(&client);
    cm::BoundedResponse response(String(expected), "application/json");
    response._respond(&request);
    drain(&response, request, client, sample.second, sample.second);
    verifyJson(client, expected);
    assert(client.maximumOffer == sample.second);
  }
}
static void delayedAck() {
  AsyncClient client; client.mss = 1436;
  AsyncWebServerRequest request(&client);
  const std::string expected = "\"" + std::string(2944, 'x') + "\"";
  cm::BoundedResponse response(String(expected), "application/json");
  response._respond(&request);
  client.advance(9000); // Beyond the former 5 s timeout, still bounded by 15 s.
  assert(!client.closed);
  drain(&response, request, client, 2872, 2872);
  verifyJson(client, expected);
  assert(client.timeoutEvents == 0);
}
static void partialAckAndTimeout() {
  AsyncClient client; client.mss = 1436;
  AsyncWebServerRequest request(&client);
  std::atomic<uint32_t> active{1};
  auto payload = std::make_shared<const String>(std::string(6000, 'x'));
  std::weak_ptr<const String> lifetime = payload;
  auto response = std::make_unique<cm::RuntimeMetaResponse>(std::move(payload), &active);
  response->_respond(&request);
  deliverAck(response.get(), request, client, 1436); // Header ACK admits two full body segments.
  assert(response->bodyAccepted() == 2872);
  deliverAck(response.get(), request, client, 97);
  assert(response->bodyAccepted() == 2969 && client.outstanding == 2872);
  const auto accepted = client.accepted;
  for (unsigned i = 0; i < 100; ++i) poll(response.get(), request, client);
  assert(client.accepted == accepted); // Polls cannot grow an unacknowledged window.
  client.onClose = [&]() { response.reset(); };
  client.advance(14999);
  assert(!client.closed && active == 1);
  client.advance(1);
  assert(client.closed && client.timeoutEvents == 1 && !response);
  assert(active == 0 && lifetime.expired());
  client.advance(20000);
  assert(client.timeoutEvents == 1);
}
static void unflushedAdmission() {
  AsyncClient client; client.mss = 1436;
  AsyncWebServerRequest request(&client);
  const std::string expected = "{\"value\":\"" + std::string(5000, 'x') + "\"}";
  cm::BoundedResponse response(String(expected), "application/json");
  client.flushResults = {false, false, true};
  response._respond(&request);
  const auto accepted = client.accepted;
  deliverAck(&response, request, client, 1436);
  assert(client.accepted == accepted && response.totalAcknowledged() == 0);
  poll(&response, request, client);
  drain(&response, request, client, 2872, 2872);
  verifyJson(client, expected); // Failed output must never duplicate accepted bytes.
}
static void pipelineBeforeFirstAck() {
  // [MOCKED!] A missing first ACK must not prevent the second segment's delivery.
  AsyncClient client; client.mss = 1436;
  AsyncWebServerRequest request(&client);
  const std::string expected = "\"" + std::string(6000, 'x') + "\"";
  cm::BoundedResponse response(String(expected), "application/json");
  response._respond(&request);
  client.receiveTransmitted();
  client.advance(1000);
  poll(&response, request, client);
  client.receiveTransmitted();
  assert(response.totalAcknowledged() == 0);
  assert(client.received.size() == 2872 && client.outstanding == 2872);
  assert(response.bodyAccepted() > 1436);
  const auto received = client.received;
  for (unsigned i = 0; i < 20; ++i) poll(&response, request, client);
  client.receiveTransmitted();
  assert(client.received == received); // Still bounded without ACK progress.
  client.advance(8000);
  assert(!client.closed);
  deliverAck(&response, request, client, 1436);
  client.receiveTransmitted();
  assert(client.received.size() == 4308 && client.outstanding == 2872);
  drain(&response, request, client, 2872, 2872);
  verifyJson(client, expected);
}
static void completePeerBodyBeforeLateAck() {
  // [MOCKED!] The traced 2803-byte body reaches the peer despite withheld body ACKs.
  AsyncClient client; client.mss = 1436;
  AsyncWebServerRequest request(&client);
  const std::string expected = "\"" + std::string(2801, 'x') + "\"";
  cm::BoundedResponse response(String(expected), "application/json");
  response._respond(&request);
  const size_t headerBytes = client.transmitted;
  deliverAck(&response, request, client, headerBytes);
  client.receiveTransmitted();
  const auto boundary = client.received.find("\r\n\r\n");
  assert(boundary != std::string::npos);
  assert(client.received.substr(boundary + 4) == expected);
  assert(response.totalAcknowledged() == headerBytes && client.outstanding == 2803);
  assert(response.state() == RESPONSE_WAIT_ACK && !client.closed);
  client.advance(9000);
  assert(!client.closed);
  deliverAck(&response, request, client, 2872);
  assert(client.closed && response.state() == RESPONSE_END);
  verifyJson(client, expected);
}
int main(int argc, char** argv) {
  if (argc > 1) {
    const std::string scenario = argv[1];
    if (scenario == "mss") mssWindow();
    else if (scenario == "bounds") windowBounds();
    else if (scenario == "delayed") delayedAck();
    else if (scenario == "timeout") partialAckAndTimeout();
    else if (scenario == "unflushed") unflushedAdmission();
    else if (scenario == "pipeline") pipelineBeforeFirstAck();
    else if (scenario == "peercomplete") completePeerBodyBeforeLateAck();
    else return 2;
    std::cout << scenario << " passed\n";
    return 0;
  }
  // [MOCKED!] Synthetic JSON through the actual production sender/platform shell.
  const std::string expected="{\"value\":\""+std::string(24000,'x')+"\",\"escaped\":\"a\\\"b\"}";
  {
    AsyncClient client; AsyncWebServerRequest request(&client);
    std::unique_ptr<cm::BoundedResponse> response;
    const char* originalBuffer=nullptr;
    {
      String generated=expected;
      originalBuffer=generated.c_str();
      response=std::make_unique<cm::BoundedResponse>(std::move(generated),"application/json");
    }
    client.acceptLimits={0,7,0,17,10000,0,9,0,23};
    client.flushResults={false,false,true,false,true};
    response->_respond(&request);
    drain(response.get(),request,client);
    verifyJson(client,expected);
    assert(std::find(client.offeredPointers.begin(),client.offeredPointers.end(),originalBuffer)!=client.offeredPointers.end());
  }
  {
    AsyncClient client; AsyncWebServerRequest request(&client);
    std::atomic<uint32_t> active{1};
    auto shared=std::make_shared<const String>(expected);
    std::weak_ptr<const String> lifetime=shared;
    const char* originalBuffer=shared->c_str();
    auto response=std::make_unique<cm::RuntimeMetaResponse>(shared,&active);
    shared.reset();
    assert(!lifetime.expired() && active==1 && response->hasPayload());
    client.flushResults={false,false,true};
    response->_respond(&request);
    const auto accepted=client.accepted;
    response->_ack(&request,0,0);
    assert(client.accepted==accepted && active==1);
    client.onClose=[&](){response.reset();};
    drain(response.get(),request,client);
    verifyJson(client,expected);
    assert(!response && lifetime.expired() && active==0);
    assert(std::find(client.offeredPointers.begin(),client.offeredPointers.end(),originalBuffer)!=client.offeredPointers.end());
  }
  {
    AsyncClient client; AsyncWebServerRequest request(&client);
    std::atomic<uint32_t> active{1};
    auto shared=std::make_shared<const String>(expected);
    std::weak_ptr<const String> lifetime=shared;
    auto response=std::make_unique<cm::RuntimeMetaResponse>(std::move(shared),&active);
    client.flushResults={false};
    response->_respond(&request);
    client.onClose=[&](){response.reset();};
    client.close(); // Peer disconnect before body delivery.
    assert(lifetime.expired() && active==0 && !response);
    client.close();
    assert(active==0);
  }
  {
    AsyncClient client; AsyncWebServerRequest request(&client);
    auto response=std::make_unique<cm::BoundedResponse>(String(expected),"application/json");
    response->_respond(&request);
    client.onClose=[&](){response.reset();};
    client.close();
    assert(!response);
  }
  {
    AsyncClient client; AsyncWebServerRequest request(&client);
    cm::BoundedResponse response(String(),"application/json");
    response._respond(&request);
    assert(!response._finished());
    drain(&response,request,client);
    verifyJson(client,"");
  }
  {
    std::atomic<uint32_t> active{1};
    { cm::RuntimeMetaResponse response(nullptr,&active); assert(!response.hasPayload()); }
    assert(active==0);
  }
  mssWindow(); windowBounds(); delayedAck(); partialAckAndTimeout(); unflushedAdmission();
  pipelineBeforeFirstAck(); completePeerBodyBeforeLateAck();
  std::cout << "Bounded JSON response passed: 6 ownership/lifetime and 7 MSS/event/timeout scenarios; "
            << expected.size() << " exact JSON bytes; admission cap 2920\n";
}
