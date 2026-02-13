#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

namespace cm::web
{
bool initRequestBodyBuffer(AsyncWebServerRequest *request, size_t expectedSize);
String *appendRequestBodyChunk(AsyncWebServerRequest *request, const uint8_t *data, size_t len, size_t index, size_t total);
void clearRequestBodyBuffer(AsyncWebServerRequest *request);
} // namespace cm::web

