#include "WebRequestBodyBuffer.h"

namespace
{
constexpr size_t kMaxJsonBodyBytes = 8 * 1024;
constexpr uintptr_t kRejectedBodyMarkerValue = 1;
constexpr const char *kAllocFailedJson = "{\"status\":\"error\",\"reason\":\"alloc_failed\"}";

void sendPayloadTooLarge(AsyncWebServerRequest *request)
{
    if (!request)
    {
        return;
    }
    request->send(413, "application/json", "{\"status\":\"error\",\"reason\":\"payload_too_large\"}");
}

void *rejectedBodyMarker()
{
    return reinterpret_cast<void *>(kRejectedBodyMarkerValue);
}

bool isRejectedBodyMarker(const void *ptr)
{
    return ptr == rejectedBodyMarker();
}
} // namespace

bool cm::web::initRequestBodyBuffer(AsyncWebServerRequest *request, size_t expectedSize)
{
    if (!request)
    {
        return false;
    }

    if (expectedSize > kMaxJsonBodyBytes)
    {
        clearRequestBodyBuffer(request);
        request->_tempObject = rejectedBodyMarker();
        sendPayloadTooLarge(request);
        return false;
    }

    clearRequestBodyBuffer(request);
    String *body = new String();
    if (!body || !body->reserve(expectedSize))
    {
        delete body;
        request->_tempObject = rejectedBodyMarker();
        request->send(500, "application/json", kAllocFailedJson);
        return false;
    }

    request->_tempObject = body;
    return true;
}

String *cm::web::appendRequestBodyChunk(AsyncWebServerRequest *request, const uint8_t *data, size_t len, size_t index, size_t total)
{
    if (!request || isRejectedBodyMarker(request->_tempObject))
    {
        return nullptr;
    }

    if (index == 0)
    {
        if (!initRequestBodyBuffer(request, total))
        {
            return nullptr;
        }
    }
    else if (!request->_tempObject)
    {
        // Ignore follow-up chunks after a rejected/failed first chunk.
        return nullptr;
    }

    if (total > kMaxJsonBodyBytes)
    {
        clearRequestBodyBuffer(request);
        request->_tempObject = rejectedBodyMarker();
        return nullptr;
    }

    String *body = static_cast<String *>(request->_tempObject);
    if (!body)
    {
        return nullptr;
    }

    if ((body->length() + len) > kMaxJsonBodyBytes)
    {
        clearRequestBodyBuffer(request);
        request->_tempObject = rejectedBodyMarker();
        sendPayloadTooLarge(request);
        return nullptr;
    }

    if (!body->concat(reinterpret_cast<const char *>(data), len))
    {
        clearRequestBodyBuffer(request);
        request->_tempObject = rejectedBodyMarker();
        request->send(500, "application/json", kAllocFailedJson);
        return nullptr;
    }

    return body;
}

void cm::web::clearRequestBodyBuffer(AsyncWebServerRequest *request)
{
    if (!request)
    {
        return;
    }

    if (request->_tempObject && !isRejectedBodyMarker(request->_tempObject))
    {
        delete static_cast<String *>(request->_tempObject);
    }
    request->_tempObject = nullptr;
}

