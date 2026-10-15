#include "web/BoundedResponse.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>

static void drain(cm::BoundedResponse* response, AsyncWebServerRequest& request, AsyncClient& client) {
  for (size_t step=0;step<10000 && !client.closed;++step) {
    const size_t acknowledged = std::min<size_t>(client.outstanding,97);
    client.outstanding -= acknowledged;
    response->_ack(&request,acknowledged,0);
    assert(client.outstanding<=512);
  }
  assert(client.closed);
  assert(client.maximumOutstanding<=512 && client.maximumOffer<=512);
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
int main() {
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
  std::cout<<"Bounded JSON response passed: 6 ownership/lifetime/transport scenarios, "<<expected.size()<<" exact JSON bytes; max outstanding512\n";
}
