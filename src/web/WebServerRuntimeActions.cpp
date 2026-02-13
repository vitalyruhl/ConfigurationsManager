#include "WebServer.h"
#include "../ConfigManager.h"
#include "WebRequestBodyBuffer.h"

namespace
{
constexpr const char *kJsonOk = "{\"status\":\"ok\"}";
constexpr const char *kJsonNoManager = "{\"status\":\"error\",\"reason\":\"no_manager\"}";
constexpr const char *kJsonInvalid = "{\"status\":\"error\",\"reason\":\"invalid_json\"}";

enum class RuntimeActionKind
{
    Button,
    Checkbox,
    StateButton,
    IntSlider,
    FloatSlider,
    IntInput,
    FloatInput,
};

bool parseFloatFromString(String valueStr, float &outValue)
{
    valueStr.replace(",", ".");
    outValue = valueStr.toFloat();
    return true;
}

bool parseQueryValue(RuntimeActionKind kind, AsyncWebServerRequest *request, bool &hasValue, bool &boolValue, int &intValue, float &floatValue)
{
    hasValue = request && request->hasParam("value");
    if (!hasValue)
    {
        return kind == RuntimeActionKind::Button || kind == RuntimeActionKind::StateButton;
    }

    String valueStr = request->getParam("value")->value();
    switch (kind)
    {
    case RuntimeActionKind::Button:
        return false;
    case RuntimeActionKind::Checkbox:
        boolValue = (valueStr == "true" || valueStr == "1");
        return true;
    case RuntimeActionKind::StateButton:
        valueStr.toLowerCase();
        boolValue = (valueStr == "true" || valueStr == "1" || valueStr == "on");
        return true;
    case RuntimeActionKind::IntSlider:
    case RuntimeActionKind::IntInput:
        intValue = valueStr.toInt();
        return true;
    case RuntimeActionKind::FloatSlider:
    case RuntimeActionKind::FloatInput:
        return parseFloatFromString(valueStr, floatValue);
    }
    return false;
}

bool parseBodyValue(RuntimeActionKind kind, JsonObject doc, bool &hasValue, bool &boolValue, int &intValue, float &floatValue)
{
    hasValue = doc.containsKey("value");
    if (!hasValue)
    {
        return kind == RuntimeActionKind::Button || kind == RuntimeActionKind::StateButton;
    }

    switch (kind)
    {
    case RuntimeActionKind::Button:
        return false;
    case RuntimeActionKind::Checkbox:
    case RuntimeActionKind::StateButton:
        boolValue = doc["value"].as<bool>();
        return true;
    case RuntimeActionKind::IntSlider:
    case RuntimeActionKind::IntInput:
        intValue = doc["value"].as<int>();
        return true;
    case RuntimeActionKind::FloatSlider:
    case RuntimeActionKind::FloatInput:
        floatValue = doc["value"].as<float>();
        return true;
    }
    return false;
}

bool dispatchRuntimeAction(ConfigManagerClass *configManager, RuntimeActionKind kind, const String &group, const String &key, bool hasValue, bool boolValue, int intValue, float floatValue)
{
    if (!configManager)
    {
        return false;
    }

    auto &runtime = configManager->getRuntimeManager();
    switch (kind)
    {
    case RuntimeActionKind::Button:
        runtime.handleButtonPress(group, key);
        return true;
    case RuntimeActionKind::Checkbox:
        if (!hasValue)
        {
            return false;
        }
        runtime.handleCheckboxChange(group, key, boolValue);
        return true;
    case RuntimeActionKind::StateButton:
        if (hasValue)
        {
            runtime.handleStateButtonSet(group, key, boolValue);
        }
        else
        {
            runtime.handleStateButtonToggle(group, key);
        }
        return true;
    case RuntimeActionKind::IntSlider:
        if (!hasValue)
        {
            return false;
        }
        runtime.handleIntSliderChange(group, key, intValue);
        return true;
    case RuntimeActionKind::FloatSlider:
        if (!hasValue)
        {
            return false;
        }
        runtime.handleFloatSliderChange(group, key, floatValue);
        return true;
    case RuntimeActionKind::IntInput:
        if (!hasValue)
        {
            return false;
        }
        runtime.handleIntInputChange(group, key, intValue);
        return true;
    case RuntimeActionKind::FloatInput:
        if (!hasValue)
        {
            return false;
        }
        runtime.handleFloatInputChange(group, key, floatValue);
        return true;
    }
    return false;
}
} // namespace

void ConfigManagerWeb::setupRuntimeActionRoutes()
{
    auto registerRoute = [this](const char *path, RuntimeActionKind kind)
    {
        server->on(path, HTTP_POST,
                   [this, kind](AsyncWebServerRequest *request)
                   {
                       if (!configManager)
                       {
                           request->send(500, "application/json", kJsonNoManager);
                           return;
                       }

                       if (request->hasParam("group") && request->hasParam("key"))
                       {
                           const String group = request->getParam("group")->value();
                           const String key = request->getParam("key")->value();
                           bool hasValue = false;
                           bool boolValue = false;
                           int intValue = 0;
                           float floatValue = 0.0f;

                           if (!parseQueryValue(kind, request, hasValue, boolValue, intValue, floatValue))
                           {
                               request->send(400, "application/json", kJsonInvalid);
                               return;
                           }

                           if (!dispatchRuntimeAction(configManager, kind, group, key, hasValue, boolValue, intValue, floatValue))
                           {
                               request->send(400, "application/json", kJsonInvalid);
                               return;
                           }

                           request->send(200, "application/json", kJsonOk);
                           return;
                       }

                       if (!cm::web::initRequestBodyBuffer(request, request->contentLength()))
                       {
                           return;
                       }
                   },
                   nullptr,
                   [this, kind](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                   {
                       String *body = cm::web::appendRequestBodyChunk(request, data, len, index, total);
                       if (!body)
                       {
                           return;
                       }

                       if (index + len != total)
                       {
                           return;
                       }

                       if (!configManager)
                       {
                           request->send(500, "application/json", kJsonNoManager);
                           cm::web::clearRequestBodyBuffer(request);
                           return;
                       }

                       DynamicJsonDocument doc(256);
                       DeserializationError error = deserializeJson(doc, *body);
                       if (error || !doc.containsKey("group") || !doc.containsKey("key"))
                       {
                           request->send(400, "application/json", kJsonInvalid);
                           cm::web::clearRequestBodyBuffer(request);
                           return;
                       }

                       const String group = doc["group"].as<String>();
                       const String key = doc["key"].as<String>();
                       bool hasValue = false;
                       bool boolValue = false;
                       int intValue = 0;
                       float floatValue = 0.0f;

                       if (!parseBodyValue(kind, doc.as<JsonObject>(), hasValue, boolValue, intValue, floatValue) ||
                           !dispatchRuntimeAction(configManager, kind, group, key, hasValue, boolValue, intValue, floatValue))
                       {
                           request->send(400, "application/json", kJsonInvalid);
                           cm::web::clearRequestBodyBuffer(request);
                           return;
                       }

                       request->send(200, "application/json", kJsonOk);
                       cm::web::clearRequestBodyBuffer(request);
                   });
    };

    registerRoute("/runtime_action/button", RuntimeActionKind::Button);
    registerRoute("/runtime_action/checkbox", RuntimeActionKind::Checkbox);
    registerRoute("/runtime_action/state_button", RuntimeActionKind::StateButton);
    registerRoute("/runtime_action/int_slider", RuntimeActionKind::IntSlider);
    registerRoute("/runtime_action/float_slider", RuntimeActionKind::FloatSlider);
    registerRoute("/runtime_action/int_input", RuntimeActionKind::IntInput);
    registerRoute("/runtime_action/float_input", RuntimeActionKind::FloatInput);
}

