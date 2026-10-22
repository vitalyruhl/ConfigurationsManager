#pragma once

#include <ArduinoJson.h>

namespace cm {
namespace detail {

// Keep the MQTT schema and publication order together while allowing host validation.
template <typename Info, typename Text, typename Connected, typename Publish>
bool publishSystemInfoPayloads(const Info& info, const Text& baseTopic, const Text& uptimeHuman, Connected connected, Publish publishPayload) {
  bool okEsp;
  {
    Text espPayload;
    {
      JsonDocument espDoc;
      espDoc["uptimeMs"] = info.uptimeMs;
      espDoc["uptimeHuman"] = uptimeHuman;
      espDoc["freeHeap"] = info.freeHeap;
      if (info.minFreeHeap > 0)
        espDoc["minFreeHeap"] = info.minFreeHeap;
      if (info.maxAllocHeap > 0)
        espDoc["maxAllocHeap"] = info.maxAllocHeap;
      espDoc["flashSizeBytes"] = info.flashSizeBytes;
      espDoc["cpuFreqMHz"] = info.cpuFreqMHz;
      espDoc["chipModel"] = info.chipModel;
      espDoc["chipRevision"] = info.chipRevision;
      espDoc["sdkVersion"] = info.sdkVersion;
      serializeJson(espDoc, espPayload);
    } // Release the document before allocating the topic or writing to MQTT.
    const Text espTopic = baseTopic + "/ESP";
    okEsp = publishPayload(espTopic, espPayload);
  } // Release the ESP payload and topic before building WiFi JSON.

  bool okWiFi;
  {
    Text wifiPayload;
    {
      JsonDocument wifiDoc;
      wifiDoc["uptimeMs"] = info.uptimeMs;
      wifiDoc["uptimeHuman"] = uptimeHuman;
      wifiDoc["hostname"] = info.hostname;
      wifiDoc["ssid"] = info.ssid;
      wifiDoc["rssi"] = info.rssi;
      wifiDoc["ip"] = info.ip;
      wifiDoc["mac"] = info.mac;
      wifiDoc["connected"] = connected();
      serializeJson(wifiDoc, wifiPayload);
    }
    const Text wifiTopic = baseTopic + "/WiFi";
    okWiFi = publishPayload(wifiTopic, wifiPayload);
  }
  return okEsp && okWiFi;
}

} // namespace detail
} // namespace cm
