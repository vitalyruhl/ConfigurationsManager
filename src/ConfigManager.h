#pragma once

#include <Arduino.h>
#include <string_view>
#include <Preferences.h>
#include <vector>
#include <functional>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <type_traits>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <exception>
#include <algorithm>
#include <cstdint>

#include "ConfigManagerConfig.h"

// Modular components
#include "wifi/WiFiManager.h"
#include "web/WebServer.h"
#include "ota/OTAManager.h"
#include "runtime/RuntimeManager.h"

#if CM_EMBED_WEBUI
#include "html_content.h" // Generated: provides WEB_HTML_GZ and accessors
#else
// External UI mode: provide a stub implementation that returns empty content
class WebHTML
{
public:
    const uint8_t *getWebHTMLGz() { return nullptr; }
    size_t getWebHTMLGzLen() { return 0; }
};
#endif

#define CONFIGMANAGER_VERSION "2.7.0" // Modular refactor

#if CM_ENABLE_THEMING && CM_ENABLE_STYLE_RULES
inline constexpr char CM_DEFAULT_RUNTIME_STYLE_CSS[] PROGMEM = R"CSS(

)CSS";
#elif CM_ENABLE_THEMING
inline constexpr char CM_DEFAULT_RUNTIME_STYLE_CSS[] PROGMEM = R"CSS(

)CSS";
#endif

// ConfigOptions must be defined before any usage in Config<T>
template <typename T>
struct ConfigOptions
{
    const char *keyName;
    const char *category;
    T defaultValue;
    const char *prettyName = nullptr;
    const char *prettyCat = nullptr;
    bool showInWeb = true;
    bool isPassword = false;
    void (*cb)(T) = nullptr;
    std::function<bool()> showIf = nullptr;
};

#if __cplusplus >= 201703L
// OptionGroup Helper for reduced boilerplate
struct OptionGroup
{
    const char *category;
    const char *prettyCat;

    template <typename T>
    constexpr ConfigOptions<T> opt(const char *keyName, T defaultValue, const char *prettyName = nullptr,
                                   bool showInWeb = true, bool isPassword = false, void (*cb)(T) = nullptr,
                                   std::function<bool()> showIf = nullptr) const
    {
        return {keyName, category, defaultValue, prettyName, prettyCat, showInWeb, isPassword, cb, showIf};
    }
};
#endif

// Server abstraction
extern AsyncWebServer server;
class ConfigManagerClass;

// Logging macros
#if CM_ENABLE_LOGGING
#define CM_LOG(...) ConfigManagerClass::log_message(__VA_ARGS__)
#if CM_ENABLE_VERBOSE_LOGGING
#define CM_LOG_VERBOSE(...) ConfigManagerClass::log_message(__VA_ARGS__)
#else
#define CM_LOG_VERBOSE(...) \
    do                      \
    {                       \
    } while (0)
#endif
#else
#define CM_LOG(...) \
    do              \
    {               \
    } while (0)
#define CM_LOG_VERBOSE(...) \
    do                      \
    {                       \
    } while (0)
#endif

// Setting types and base classes (keeping existing implementation)
enum class SettingType
{
    BOOL,
    INT,
    FLOAT,
    STRING,
    PASSWORD
};

template <typename T>
struct TypeTraits
{
};
template <>
struct TypeTraits<bool>
{
    static constexpr SettingType type = SettingType::BOOL;
};
template <>
struct TypeTraits<int>
{
    static constexpr SettingType type = SettingType::INT;
};
template <>
struct TypeTraits<float>
{
    static constexpr SettingType type = SettingType::FLOAT;
};
template <>
struct TypeTraits<String>
{
    static constexpr SettingType type = SettingType::STRING;
};

using LogCallback = std::function<void(const char *, ...)>;

template <size_t N>
constexpr size_t string_literal_length(const char (&)[N])
{
    return N - 1;
}

constexpr size_t const_strlen(const char *s)
{
    size_t len = 0;
    while (*s)
    {
        ++len;
        ++s;
    }
    return len;
}

// BaseSetting class (keeping existing implementation)
class BaseSetting
{
protected:
    bool showInWeb;
    bool isPassword;
    bool modified = false;
    const char *keyName;
    const char *category;
    const char *displayName;
    const char *categoryPretty = nullptr;
    SettingType type;
    bool hasKeyLengthError = false;
    String keyLengthErrorMsg;

    static constexpr size_t MAX_KEY_COMBINED_LEN = 14;
    static constexpr size_t MAX_PREFS_TOTAL_LEN = 15;

    mutable std::function<void(const char *)> logger;

    void log(const char *format, ...) const
    {
#if CM_ENABLE_LOGGING
        if (logger)
        {
            char buffer[256];
            va_list args;
            va_start(args, format);
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            logger(buffer);
        }
#endif
    }

    void checkKeyLength()
    {
        if (hasKeyLengthError)
            return;

        const size_t catLen = const_strlen(category);
        const size_t keyLen = const_strlen(keyName);
        const size_t totalLen = catLen + keyLen;

        if (totalLen > MAX_KEY_COMBINED_LEN)
        {
            hasKeyLengthError = true;
            keyLengthErrorMsg = String("Key too long: ") + category + "." + keyName +
                                " (" + String(totalLen) + " > " + String(MAX_KEY_COMBINED_LEN) + ")";
            return;
        }

        for (size_t i = 0; i < catLen; i++)
        {
            if (category[i] == ' ' || category[i] == '\t' || category[i] == '\n')
            {
                hasKeyLengthError = true;
                keyLengthErrorMsg = String("Category contains whitespace: ") + category;
                return;
            }
        }

        for (size_t i = 0; i < keyLen; i++)
        {
            if (keyName[i] == ' ' || keyName[i] == '\t' || keyName[i] == '\n')
            {
                hasKeyLengthError = true;
                keyLengthErrorMsg = String("Key contains whitespace: ") + keyName;
                return;
            }
        }
    }

public:
    bool hasError() const { return hasKeyLengthError; }
    const char *getError() const { return keyLengthErrorMsg.c_str(); }

    void setLogger(std::function<void(const char *)> logFunc)
    {
        logger = logFunc;
    }

    BaseSetting(const char *category, const char *keyName, const char *displayName, SettingType type, bool showInWeb = true, bool isPassword = false)
        : keyName(keyName), category(category), displayName(displayName), type(type), showInWeb(showInWeb), isPassword(isPassword)
    {
        checkKeyLength();
    }

    BaseSetting(const char *category, const char *keyName, const char *displayName, const char *categoryPretty, SettingType type, bool showInWeb = true, bool isPassword = false)
        : keyName(keyName), category(category), displayName(displayName), categoryPretty(categoryPretty), type(type), showInWeb(showInWeb), isPassword(isPassword)
    {
        checkKeyLength();
    }

    template <size_t CatLen, size_t KeyLen>
    BaseSetting(const char (&cat)[CatLen], const char (&key)[KeyLen], const char *displayName, SettingType type, bool showInWeb = true, bool isPassword = false)
        : BaseSetting(cat, key, displayName, type, showInWeb, isPassword) {}

    template <size_t CatLen, size_t KeyLen>
    BaseSetting(const char (&cat)[CatLen], const char (&key)[KeyLen], const char *displayName, const char *categoryPretty, SettingType type, bool showInWeb = true, bool isPassword = false)
        : BaseSetting(cat, key, displayName, categoryPretty, type, showInWeb, isPassword) {}

    const char *getCategoryPretty() const { return categoryPretty; }

    virtual ~BaseSetting() = default;
    virtual SettingType getType() const = 0;
    virtual void load(Preferences &prefs) = 0;
    virtual void save(Preferences &prefs) = 0;
    virtual void setDefault() = 0;
    virtual void toJSON(JsonObject &obj) const = 0;
    virtual bool fromJSON(const JsonVariant &value) = 0;
    virtual bool isVisible() const { return showInWeb; }

    const char *getDisplayName() const
    {
        return displayName ? displayName : keyName;
    }

    mutable char keyBuffer[16];
    const char *getKey() const
    {
        // Implementation of key generation logic (keeping existing)
        const size_t catLen = const_strlen(category);
        const size_t keyLen = const_strlen(keyName);

        if (catLen + keyLen + 1 > 15)
        {
            size_t totalAvailable = 14;
            size_t catPart = std::min(catLen, totalAvailable / 2);
            size_t keyPart = totalAvailable - catPart;

            if (keyLen <= keyPart)
            {
                catPart = totalAvailable - keyLen;
                keyPart = keyLen;
            }
            else if (catLen <= catPart)
            {
                keyPart = totalAvailable - catLen;
                catPart = catLen;
            }

            strncpy(keyBuffer, category, catPart);
            keyBuffer[catPart] = '_';
            strncpy(keyBuffer + catPart + 1, keyName, keyPart);
            keyBuffer[catPart + 1 + keyPart] = '\0';
        }
        else
        {
            strcpy(keyBuffer, category);
            strcat(keyBuffer, "_");
            strcat(keyBuffer, keyName);
        }

        return keyBuffer;
    }

    bool isSecret() const { return isPassword; }
    const char *getCategory() const { return category; }
    const char *getName() const { return keyName; }
    bool shouldShowInWeb() const { return showInWeb; }
    bool needsSave() const { return modified; }
};

// Config<T> template class (keeping existing implementation)
template <typename T>
class Config : public BaseSetting
{
private:
    T value;
    T defaultValue;
    void (*originalCallback)(T);
    std::function<void(T)> callback = nullptr;

public:
#if CM_ENABLE_DYNAMIC_VISIBILITY
    std::function<bool()> showIfFunc = nullptr;
    bool isVisible() const override
    {
        return showIfFunc ? showIfFunc() : BaseSetting::isVisible();
    }
#else
    bool isVisible() const override { return BaseSetting::isVisible(); }
#endif

    // Constructor implementations (keeping existing)
    explicit Config(const ConfigOptions<T> &opts)
        : BaseSetting(opts.category, opts.keyName, opts.prettyName, opts.prettyCat, TypeTraits<T>::type, opts.showInWeb, opts.isPassword),
          value(opts.defaultValue), defaultValue(opts.defaultValue), originalCallback(opts.cb)
    {
#if CM_ENABLE_DYNAMIC_VISIBILITY
        showIfFunc = opts.showIf;
#endif
        if (originalCallback)
        {
            callback = [this](T newValue)
            { originalCallback(newValue); };
        }
    }

    // Legacy constructors for backward compatibility
    Config(const char *keyName, const char *category, T defaultValue, const char *displayName = nullptr, const char *prettyCat = nullptr)
        : BaseSetting(category, keyName, displayName ? displayName : keyName, prettyCat, TypeTraits<T>::type, true, false),
          value(defaultValue), defaultValue(defaultValue), originalCallback(nullptr)
    {
    }

    Config(const char *category, const char *keyName, T defaultValue, const char *displayName = nullptr, bool showInWeb = true, bool isPassword = false, void (*cb)(T) = nullptr)
        : BaseSetting(category, keyName, displayName ? displayName : keyName, TypeTraits<T>::type, showInWeb, isPassword),
          value(defaultValue), defaultValue(defaultValue), originalCallback(cb)
    {
        if (originalCallback)
        {
            callback = [this](T newValue)
            { originalCallback(newValue); };
        }
    }

    T get() const { return value; }
    void set(const T &newValue)
    {
        if (value != newValue)
        {
            value = newValue;
            modified = true;
            if (callback)
                callback(newValue);
        }
    }

    void setCallback(std::function<void(T)> cb) { callback = cb; }

    SettingType getType() const override { return TypeTraits<T>::type; }

    void load(Preferences &prefs) override
    {
        if constexpr (std::is_same_v<T, String>)
        {
            value = prefs.getString(getKey(), defaultValue);
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            value = prefs.getBool(getKey(), defaultValue);
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            value = prefs.getInt(getKey(), defaultValue);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            value = prefs.getFloat(getKey(), defaultValue);
        }
        modified = false;
    }

    void save(Preferences &prefs) override
    {
        if (!modified)
            return;

        if constexpr (std::is_same_v<T, String>)
        {
            prefs.putString(getKey(), value);
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            prefs.putBool(getKey(), value);
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            prefs.putInt(getKey(), value);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            prefs.putFloat(getKey(), value);
        }
        modified = false;
    }

    void setDefault() override
    {
        set(defaultValue);
    }

    void toJSON(JsonObject &obj) const override
    {
        if (isSecret())
        {
            obj[getName()] = "***";
        }
        else
        {
            obj[getName()] = value;
        }
    }

    bool fromJSON(const JsonVariant &jsonValue) override
    {
        if (jsonValue.isNull())
            return false;

        if constexpr (std::is_same_v<T, String>)
        {
            if (jsonValue.is<const char *>())
            {
                set(String(jsonValue.as<const char *>()));
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            if (jsonValue.is<bool>())
            {
                set(jsonValue.as<bool>());
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            if (jsonValue.is<int>())
            {
                set(jsonValue.as<int>());
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            if (jsonValue.is<float>())
            {
                set(jsonValue.as<float>());
                return true;
            }
        }
        return false;
    }
};

// Visibility helper factories
#if CM_ENABLE_DYNAMIC_VISIBILITY
inline std::function<bool()> showIfTrue(const Config<bool> &flag)
{
    return [&flag]()
    { return flag.get(); };
}
inline std::function<bool()> showIfFalse(const Config<bool> &flag)
{
    return [&flag]()
    { return !flag.get(); };
}
#else
inline std::function<bool()> showIfTrue(const Config<bool> &)
{
    return []()
    { return true; };
}
inline std::function<bool()> showIfFalse(const Config<bool> &)
{
    return []()
    { return true; };
}
#endif

// Main ConfigManager class - now modular and much smaller!
class ConfigManagerClass
{
public:
    typedef std::function<void(const char *)> LogCallback;

private:
    Preferences prefs;
    std::vector<BaseSetting *> settings;
    String appName;

    // Modular components
    ConfigManagerWiFi wifiManager;
    ConfigManagerWeb webManager;
    ConfigManagerOTA otaManager;
    ConfigManagerRuntime runtimeManager;

    // WebSocket support
#if CM_ENABLE_WS_PUSH
    AsyncWebSocket *ws = nullptr;
    bool wsInitialized = false;
    bool wsEnabled = false;
    bool pushOnConnect = true;
    uint32_t wsInterval = 2000;
    unsigned long wsLastPush = 0;
    std::function<String()> customPayloadBuilder;
#endif

public:
    ConfigManagerClass()
    {
        webManager.setCallbacks(
            [this]()
            { return toJSON(); }, // config JSON
            [this]()
            { return runtimeManager.runtimeValuesToJSON(); }, // runtime JSON
            [this]()
            { return runtimeManager.runtimeMetaToJSON(); }, // runtime meta JSON
            [this]()
            { reboot(); }, // reboot callback
            [this]()
            { for (auto *s : settings) s->setDefault(); saveAll(); }, // reset callback
            [this](const String &group, const String &key, const String &value) -> bool
            {
                return updateSetting(group, key, value);
            });
    }

    // Settings management
    BaseSetting *findSetting(const String &category, const String &key)
    {
        for (auto *setting : settings)
        {
            if (String(setting->getCategory()) == category && String(setting->getName()) == key)
            {
                return setting;
            }
        }
        return nullptr;
    }

    void addSetting(BaseSetting *setting)
    {
        if (setting->hasError())
        {
            CM_LOG("[E] Setting error: %s", setting->getError());
            return;
        }
        settings.push_back(setting);
        setting->setLogger([](const char *msg)
                           { CM_LOG("%s", msg); });
    }

    void loadAll()
    {
        if (!prefs.begin("ConfigManager", false))
        {
            throw std::runtime_error("Failed to initialize preferences");
        }

        for (auto *s : settings)
        {
            s->load(prefs);
        }
        prefs.end();
    }

    void saveAll()
    {
        if (!prefs.begin("ConfigManager", false))
        {
            CM_LOG("[E] Failed to open preferences for writing");
            return;
        }

        for (auto *s : settings)
        {
            if (s->needsSave())
            {
                s->save(prefs);
            }
        }
        prefs.end();
    }

    bool updateSetting(const String &category, const String &key, const String &value)
    {
        BaseSetting *setting = findSetting(category, key);
        if (!setting)
            return false;

        DynamicJsonDocument doc(256);
        doc.set(value);
        return setting->fromJSON(doc.as<JsonVariant>());
    }

    void checkSettingsForErrors()
    {
        for (auto *s : settings)
        {
            if (s->hasError())
            {
                CM_LOG("[E] Setting error: %s", s->getError());
            }
        }
    }

    // App management
    void setAppName(const String &name)
    {
        appName = name;
        CM_LOG("[I] App name set: %s", name.c_str());
    }

    const String &getAppName() const { return appName; }

    // WiFi management - NON-BLOCKING!
    void startWebServer(const String &ssid, const String &password)
    {
        CM_LOG("[I] Starting web server with DHCP connection to %s", ssid.c_str());

        // Start WiFi connection non-blocking
        wifiManager.begin(10000, 30); // 10s reconnect interval, 30min auto-reboot timeout
        wifiManager.setCallbacks(
            [this]()
            {
                CM_LOG("[WiFi] Connected! Starting web server...");
                webManager.defineAllRoutes();
                otaManager.setupWebRoutes(webManager.getServer());
                // Call external connected callback if available
                extern void onWiFiConnected();
                onWiFiConnected();
            },
            [this]()
            { 
                CM_LOG("[WiFi] Disconnected"); 
                // Call external disconnected callback if available
                extern void onWiFiDisconnected();
                onWiFiDisconnected();
            },
            [this]()
            { 
                CM_LOG("[WiFi] AP Mode active"); 
                // Call external AP mode callback if available
                extern void onWiFiAPMode();
                onWiFiAPMode();
            });
        wifiManager.startConnection(ssid, password);

        // Initialize modules
        webManager.begin(this);
        otaManager.begin(this);
        runtimeManager.begin(this);

        CM_LOG("[I] ConfigManager modules initialized - WiFi connecting in background");
    }

    void startWebServer(const IPAddress &staticIP, const IPAddress &gateway, const IPAddress &subnet, const String &ssid, const String &password)
    {
        CM_LOG("[I] Starting web server with static IP %s", staticIP.toString().c_str());

        wifiManager.begin(10000, 30); // 10s reconnect interval, 30min auto-reboot timeout
        wifiManager.setCallbacks(
            [this]()
            {
                CM_LOG("[WiFi] Connected! Starting web server...");
                webManager.defineAllRoutes();
                otaManager.setupWebRoutes(webManager.getServer());
                // Call external connected callback if available
                extern void onWiFiConnected();
                onWiFiConnected();
            },
            [this]()
            { 
                CM_LOG("[WiFi] Disconnected"); 
                // Call external disconnected callback if available
                extern void onWiFiDisconnected();
                onWiFiDisconnected();
            },
            [this]()
            { 
                CM_LOG("[WiFi] AP Mode active"); 
                // Call external AP mode callback if available
                extern void onWiFiAPMode();
                onWiFiAPMode();
            });
        wifiManager.startConnection(staticIP, gateway, subnet, ssid, password);

        webManager.begin(this);
        otaManager.begin(this);
        runtimeManager.begin(this);
    }

    void startAccessPoint(const String &apSSID = "", const String &apPassword = "")
    {
        String ssid = apSSID.isEmpty() ? (appName + "_AP") : apSSID;
        CM_LOG("[I] Starting Access Point: %s", ssid.c_str());

        wifiManager.begin();
        wifiManager.startAccessPoint(ssid, apPassword);

        webManager.begin(this);
        webManager.defineAllRoutes();
        otaManager.begin(this);
        otaManager.setupWebRoutes(webManager.getServer());
        runtimeManager.begin(this);
    }

    // Non-blocking client handling
    void handleClient() { /* no-op for async */ }

    // WiFi status and control
    bool getWiFiStatus() { return wifiManager.isConnected(); }
    void reconnectWifi() { wifiManager.reconnect(); }

    // System control
    void reboot()
    {
        CM_LOG_VERBOSE("[R] Rebooting...");
        delay(1000);
        ESP.restart();
    }

    // Legacy methods for backward compatibility (simplified)
    void setCustomCss(const char *css, size_t len)
    {
        // Note: setCustomCss not implemented in new WebManager, skip for now
        CM_LOG("[W] setCustomCss not implemented in modular WebManager");
    }

    void clearAllFromPrefs()
    {
        for (auto *setting : settings)
        {
            setting->setDefault(); // Reset to default values
        }
        CM_LOG("[I] All settings cleared from preferences");
    }

    // Runtime methods - simplified for new modular structure
    void defineRuntimeField(const String &category, const String &key, const String &name, const String &unit, float min, float max)
    {
        // Note: Use addRuntimeMeta directly instead
        CM_LOG("[W] defineRuntimeField not implemented, use addRuntimeMeta directly");
    }

    void defineRuntimeStateButton(const String &category, const String &key, const String &name, std::function<bool()> getter, std::function<void(bool)> setter)
    {
        // Note: State buttons not implemented in new RuntimeManager
        CM_LOG("[W] defineRuntimeStateButton not implemented in modular RuntimeManager");
    }

    void defineRuntimeBool(const String &category, const String &key, const String &name, bool defaultValue, int order = 0)
    {
        // Note: Use addRuntimeMeta directly instead
        CM_LOG("[W] defineRuntimeBool not implemented, use addRuntimeMeta directly");
    }

    void defineRuntimeAlarm(const String &category, const String &key, const String &name, std::function<bool()> condition, std::function<void()> onTrigger = nullptr, std::function<void()> onClear = nullptr)
    {
        // Note: Use addRuntimeAlarm directly instead
        CM_LOG("[W] defineRuntimeAlarm not implemented, use addRuntimeAlarm directly");
    }

    void defineRuntimeFloatSlider(const String &category, const String &key, const String &name, float min, float max, float &variable, int precision = 1, std::function<void()> onChange = nullptr)
    {
        // Note: Float sliders not implemented in new RuntimeManager
        CM_LOG("[W] defineRuntimeFloatSlider not implemented in modular RuntimeManager");
    }

    void handleRuntimeAlarms()
    {
        runtimeManager.updateAlarms(); // Use updateAlarms() instead of handleRuntimeAlarms()
    }

    bool isOTAInitialized()
    {
        return otaManager.isInitialized(); // Use public method
    }

    void stopOTA()
    {
        // Note: stop() method not implemented in new OTAManager
        CM_LOG("[W] stopOTA not implemented in modular OTAManager");
    }

    // JSON export
    String toJSON(bool includeSecrets = false)
    {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();

        String catNames[20];
        int catCount = 0;
        for (auto *s : settings)
        {
            if (!s->isVisible())
                continue;

            const char *category = s->getCategory();
            JsonObject catObj;

            bool foundCat = false;
            for (int i = 0; i < catCount; ++i)
            {
                if (catNames[i] == category)
                {
                    catObj = root[category];
                    foundCat = true;
                    break;
                }
            }

            if (!foundCat)
            {
                catObj = root.createNestedObject(category);
                catNames[catCount++] = category;

                const char *prettyName = s->getCategoryPretty();
                if (prettyName)
                {
                    catObj["_categoryName"] = prettyName;
                }
            }

            if (includeSecrets || !s->isSecret())
            {
                s->toJSON(catObj);
            }
        }

        String output;
        serializeJsonPretty(doc, output);
        return output;
    }

    // Runtime providers
    void addRuntimeProvider(const RuntimeValueProvider &provider)
    {
        runtimeManager.addRuntimeProvider(provider);
    }
    void addRuntimeProvider(const String &name, std::function<void(JsonObject &)> fillFunc, int order = 100)
    {
        runtimeManager.addRuntimeProvider(name, fillFunc, order);
    }

    // OTA management
#if CM_ENABLE_OTA
    void setupOTA(const String &hostname, const String &password = "")
    {
        otaManager.setup(hostname, password);
    }
    void handleOTA() { otaManager.handle(); }
    void enableOTA(bool enabled = true) { otaManager.enable(enabled); }
#else
    void setupOTA(const String &, const String & = "") {}
    void handleOTA() {}
    void enableOTA(bool = true) {}
#endif

    // System provider
    void enableBuiltinSystemProvider() { runtimeManager.enableBuiltinSystemProvider(); }
    void updateLoopTiming() { runtimeManager.updateLoopTiming(); }

    // WebSocket push
#if CM_ENABLE_WS_PUSH
    void handleWebsocketPush()
    {
        if (!wsEnabled)
            return;
        unsigned long now = millis();
        if (now - wsLastPush < wsInterval)
            return;
        wsLastPush = now;

        String payload;
        if (customPayloadBuilder)
        {
            payload = customPayloadBuilder();
        }
        else
        {
            payload = runtimeManager.runtimeValuesToJSON();
        }

        if (ws)
        {
            ws->textAll(payload);
        }
    }

    void enableWebSocketPush(uint32_t intervalMs = 2000)
    {
        if (!wsInitialized)
        {
            ws = new AsyncWebSocket("/ws");
            ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
                        {
                if (type == WS_EVT_CONNECT) {
                    CM_LOG_VERBOSE("[WS] Client connect %u", client->id());
                    if (pushOnConnect) handleWebsocketPush();
                }
                else if (type == WS_EVT_DISCONNECT) {
                    CM_LOG_VERBOSE("[WS] Client disconnect %u", client->id());
                } });
            webManager.getServer()->addHandler(ws);
            wsInitialized = true;
            CM_LOG_VERBOSE("[WS] Handler registered");
        }
        wsInterval = intervalMs;
        wsEnabled = true;
        CM_LOG_VERBOSE("[WS] Push enabled i=%lu ms", (unsigned long)wsInterval);
    }

    void disableWebSocketPush() { wsEnabled = false; }
    void setWebSocketInterval(uint32_t intervalMs) { wsInterval = intervalMs; }
    void setPushOnConnect(bool v) { pushOnConnect = v; }
    void setCustomLivePayloadBuilder(std::function<String()> fn) { customPayloadBuilder = fn; }
#else
    void handleWebsocketPush() {}
    void enableWebSocketPush(uint32_t = 2000) {}
    void disableWebSocketPush() {}
    void setWebSocketInterval(uint32_t) {}
    void setPushOnConnect(bool) {}
    void setCustomLivePayloadBuilder(std::function<String()>) {}
#endif

    // Logging
#if CM_ENABLE_LOGGING
    static void setLogger(LogCallback cb);
    static void log_message(const char *format, ...);
#else
    static void setLogger(LogCallback) {}
    static void log_message(const char *, ...) {}
#endif

    void triggerLoggerTest() { CM_LOG("[I] Logger test message"); }

    // Access to modules for advanced usage
    ConfigManagerWiFi &getWiFiManager() { return wifiManager; }
    ConfigManagerWeb &getWebManager() { return webManager; }
    ConfigManagerOTA &getOTAManager() { return otaManager; }
    ConfigManagerRuntime &getRuntimeManager() { return runtimeManager; }

public:
    static LogCallback logger;
    WebHTML webhtml;
};

extern ConfigManagerClass ConfigManager;