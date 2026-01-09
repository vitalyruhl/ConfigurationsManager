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

#define CONFIGMANAGER_VERSION "3.0.0" // Synced to library.json

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
    // Required fields
    const char *key = nullptr;           // Storage key for preferences (if nullptr, auto-generated from name+category)
    const char *name;                    // Display name in Settings UI
    const char *category;                // Card name in Settings UI
    T defaultValue;                      // Default value

    // Optional fields
    bool showInWeb = true;               // Show in web interface
    bool isPassword = false;             // Hide value (password field)
    int sortOrder = 100;                 // Sort order in GUI (lower = higher priority)
    void (*callback)(T) = nullptr;       // Value change callback
    std::function<bool()> showIf = nullptr; // Conditional visibility
    const char *categoryPretty = nullptr;   // Optional pretty name for category/card
};

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

// Helper function to generate a key from name and category
inline String generateKeyFromNameAndCategory(const char* name, const char* category) {
    String result;

    // Convert category to lowercase and remove special chars
    String catPart = String(category);
    catPart.toLowerCase();
    catPart.replace(" ", "");
    catPart.replace("-", "");
    catPart.replace("_", "");

    // Convert name to lowercase and remove special chars
    String namePart = String(name);
    namePart.toLowerCase();
    namePart.replace(" ", "");
    namePart.replace("-", "");
    namePart.replace("_", "");

    // Combine with underscore
    result = catPart + "_" + namePart;

    // Truncate to 14 chars max (ESP32 preferences key limit)
    if (result.length() > 14) {
        // Try to keep both parts balanced
        int catLen = catPart.length();
        int nameLen = namePart.length();

        if (catLen > 7 && nameLen > 7) {
            // Both long - truncate both
            result = catPart.substring(0, 6) + "_" + namePart.substring(0, 7);
        } else if (catLen > 7) {
            // Category too long
            int availableForCat = 14 - 1 - nameLen; // -1 for underscore
            result = catPart.substring(0, availableForCat) + "_" + namePart;
        } else {
            // Name too long
            int availableForName = 14 - 1 - catLen; // -1 for underscore
            result = catPart + "_" + namePart.substring(0, availableForName);
        }
    }

    return result;
}



// BaseSetting class (updated for new ConfigOptions structure)
class BaseSetting
{
protected:
    bool showInWeb;
    bool isPassword;
    bool modified = false;
    String generatedKey; // Store generated key if needed
    const char *keyName;
    const char *category;
    const char *displayName;
    const char *categoryPrettyName = nullptr;
    SettingType type;
    int sortOrder = 100;
    bool hasKeyLengthError = false;
    String keyLengthErrorMsg;

    static constexpr size_t MAX_PREFS_KEY_LEN = 15;

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

    void logVerbose(const char *format, ...) const
    {
#if CM_ENABLE_VERBOSE_LOGGING
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

        const char* actualKey = getKey();
        size_t keyLen = strlen(actualKey);

        if (keyLen > MAX_PREFS_KEY_LEN) {
            hasKeyLengthError = true;
            keyLengthErrorMsg = String("Generated key too long: ") + actualKey +
                               " (" + String(keyLen) + " > " + String(MAX_PREFS_KEY_LEN) + ") - value will not be stored!";
            log("[ERROR] %s", keyLengthErrorMsg.c_str());
            return;
        }

        // Check for invalid characters
        for (size_t i = 0; i < keyLen; i++) {
            char c = actualKey[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                hasKeyLengthError = true;
                keyLengthErrorMsg = String("Key contains invalid characters: ") + actualKey;
                log("[ERROR] %s", keyLengthErrorMsg.c_str());
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

    // New constructor for ConfigOptions-based initialization
    BaseSetting(const char* key, const char* name, const char* category, SettingType type,
                bool showInWeb = true, bool isPassword = false, int sortOrder = 100, const char* categoryPretty = nullptr)
        : keyName(key), displayName(name), category(category), categoryPrettyName(categoryPretty), type(type),
          showInWeb(showInWeb), isPassword(isPassword), sortOrder(sortOrder)
    {
        // If no key provided, generate one from name and category
        if (!keyName || strlen(keyName) == 0) {
            generatedKey = generateKeyFromNameAndCategory(name, category);
            keyName = generatedKey.c_str();
            log("[INFO] Auto-generated key '%s' for setting '%s' in category '%s'", keyName, name, category);
        }
        checkKeyLength();
    }

    virtual ~BaseSetting() = default;
    virtual SettingType getType() const = 0;
    virtual void load(Preferences &prefs) = 0;
    virtual void save(Preferences &prefs) = 0;
    virtual void setDefault() = 0;
    virtual void toJSON(JsonObject &obj) const = 0;
    virtual bool fromJSON(const JsonVariant &value) = 0;
    virtual bool isVisible() const { return showInWeb; }

    const char *getDisplayName() const { return displayName; }
    const char *getKey() const { return keyName; }
    const char *getCategory() const { return category; }
    const char *getCategoryPretty() const { return categoryPrettyName ? categoryPrettyName : category; }
    const char *getName() const { return keyName; }
    int getSortOrder() const { return sortOrder; }
    bool isSecret() const { return isPassword; }
    bool shouldShowInWeb() const { return showInWeb; }
    bool needsSave() const { return modified; }
};

// Config<T> template class (updated for new ConfigOptions structure)
template <typename T>
class Config : public BaseSetting
{
private:
    T value;
    T defaultValue;
    std::function<void(T)> callback = nullptr;

public:
    std::function<bool()> showIfFunc = nullptr;
    bool isVisible() const override
    {
        return showIfFunc ? showIfFunc() : BaseSetting::isVisible();
    }

    // New primary constructor for ConfigOptions
    explicit Config(const ConfigOptions<T> &opts)
        : BaseSetting(opts.key, opts.name, opts.category, TypeTraits<T>::type,
                     opts.showInWeb, opts.isPassword, opts.sortOrder, opts.categoryPretty),
          value(opts.defaultValue), defaultValue(opts.defaultValue)
    {
        showIfFunc = opts.showIf;
        if (opts.callback)
        {
            callback = [opts](T newValue) { opts.callback(newValue); };
        }
    }

    const T& get() const { return value; }

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
        if (hasError()) {
            log("[ERROR] Skipping load for setting '%s' due to key error: %s", getDisplayName(), getError());
            return;
        }

        // Automatically persist default values the first time a key is missing
        bool keyExists = prefs.isKey(getKey());
        logVerbose("[PREFS] Key %s.%s exists? %s", getCategory(), getKey(), keyExists ? "true" : "false");

        if (!keyExists)
        {
            value = defaultValue;

            if constexpr (std::is_same_v<T, String>)
            {
                prefs.putString(getKey(), value);
                if (isPassword)
                {
                    log("[PREFS] Initialized %s.%s = '***' (hidden) (default)", getCategory(), getKey());
                }
                else
                {
                    log("[PREFS] Initialized %s.%s = '%s' (default)", getCategory(), getKey(), value.c_str());
                }
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                prefs.putBool(getKey(), value);
                log("[PREFS] Initialized %s.%s = %s (default)", getCategory(), getKey(), value ? "true" : "false");
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                prefs.putInt(getKey(), value);
                log("[PREFS] Initialized %s.%s = %d (default)", getCategory(), getKey(), value);
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                prefs.putFloat(getKey(), value);
                log("[PREFS] Initialized %s.%s = %.2f (default)", getCategory(), getKey(), value);
            }

            modified = false;
            return;
        }

        T loadedValue;
        if constexpr (std::is_same_v<T, String>)
        {
            loadedValue = prefs.getString(getKey(), defaultValue);
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            loadedValue = prefs.getBool(getKey(), defaultValue);
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            loadedValue = prefs.getInt(getKey(), defaultValue);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            loadedValue = prefs.getFloat(getKey(), defaultValue);
        }

        value = loadedValue;
        modified = false;

        // Verbose logging for load operations
        if constexpr (std::is_same_v<T, String>)
        {
            if (isPassword) {
                logVerbose("[PREFS] Loaded %s.%s = '***' (hidden)", getCategory(), getKey());
            } else {
                logVerbose("[PREFS] Loaded %s.%s = '%s'", getCategory(), getKey(), value.c_str());
            }
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            logVerbose("[PREFS] Loaded %s.%s = %s", getCategory(), getKey(), value ? "true" : "false");
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            logVerbose("[PREFS] Loaded %s.%s = %d", getCategory(), getKey(), value);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            logVerbose("[PREFS] Loaded %s.%s = %.2f", getCategory(), getKey(), value);
        }
    }

    void save(Preferences &prefs) override
    {
        if (hasError()) {
            log("[ERROR] Skipping save for setting '%s' due to key error: %s", getDisplayName(), getError());
            return;
        }

        if (!modified) return;

        if constexpr (std::is_same_v<T, String>)
        {
            prefs.putString(getKey(), value);
            if (isPassword) {
                logVerbose("[PREFS] Saved %s.%s = '***' (hidden)", getCategory(), getKey());
            } else {
                logVerbose("[PREFS] Saved %s.%s = '%s'", getCategory(), getKey(), value.c_str());
            }
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            prefs.putBool(getKey(), value);
            logVerbose("[PREFS] Saved %s.%s = %s", getCategory(), getKey(), value ? "true" : "false");
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            prefs.putInt(getKey(), value);
            logVerbose("[PREFS] Saved %s.%s = %d", getCategory(), getKey(), value);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            prefs.putFloat(getKey(), value);
            logVerbose("[PREFS] Saved %s.%s = %.2f", getCategory(), getKey(), value);
        }

        modified = false;
    }

    void setDefault() override
    {
        value = defaultValue;
        modified = true;
    }

    void toJSON(JsonObject &obj) const override
    {
        JsonObject settingObj = obj.createNestedObject(getDisplayName());

        if (isPassword)
        {
            // Passwords are masked in config.json. Use /config/password after auth to reveal.
            settingObj["value"] = "***";
        }
        else
        {
            settingObj["value"] = value;
        }

        // Add metadata for web interface
        settingObj["displayName"] = getDisplayName();
        settingObj["isPassword"] = isPassword;
        settingObj["sortOrder"] = sortOrder;

        // Add showIf result if function is defined
        if (showIfFunc != nullptr)
        {
            settingObj["showIf"] = showIfFunc();
        }
    }

    bool fromJSON(const JsonVariant &jsonValue) override
    {
        if (jsonValue.isNull()) {
            return false;
        }

        T newValue;
        if constexpr (std::is_same_v<T, String>)
        {
            if (!jsonValue.is<const char*>()) {
                return false;
            }
            newValue = jsonValue.as<String>();
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            if (!jsonValue.is<bool>()) {
                return false;
            }
            newValue = jsonValue.as<bool>();
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            if (!jsonValue.is<int>()) return false;
            newValue = jsonValue.as<int>();
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            if (!jsonValue.is<float>()) return false;
            newValue = jsonValue.as<float>();
        }

        set(newValue);
        return true;
    }
};

// Visibility helper factories
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

// New template-based helper functions for conditional visibility
template<typename T>
std::function<bool()> showIfTrue(const Config<T>& setting) {
    return [&setting]() -> bool {
        if constexpr (std::is_same_v<T, bool>) {
            return setting.get();
        }
        return true; // For non-bool types, always show
    };
}

template<typename T>
std::function<bool()> showIfFalse(const Config<T>& setting) {
    return [&setting]() -> bool {
        if constexpr (std::is_same_v<T, bool>) {
            return !setting.get();
        }
        return true; // For non-bool types, always show
    };
}

// Main ConfigManager class - now modular and much smaller!
class ConfigManagerClass
{
public:
    typedef std::function<void(const char *)> LogCallback;

private:
    Preferences prefs;
    std::vector<BaseSetting *> settings;
    String appName;
    String appVersion;

    // Modular components
    ConfigManagerWiFi wifiManager;
    ConfigManagerWeb webManager;
    ConfigManagerOTA otaManager;
    ConfigManagerRuntime runtimeManager;

    // Optional user-provided global CSS to inject into the Web UI
    const char* customCss = nullptr;
    size_t customCssLen = 0; // 0 -> treat as null-terminated string

    // WebSocket support
#if CM_ENABLE_WS_PUSH
    AsyncWebSocket *ws = nullptr;
    bool wsInitialized = false;
    bool wsEnabled = false;
    bool pushOnConnect = true;
    uint32_t wsInterval = 2000;
    unsigned long wsLastPush = 0;
    std::function<String()> customPayloadBuilder;
    // Manual heartbeat tracking
    struct WsClientInfo { uint32_t id; unsigned long lastSeen; };
    std::vector<WsClientInfo> wsClients;
    unsigned long wsLastHeartbeat = 0;
    uint32_t wsHeartbeatInterval = 30000; // ms - ping every 30 seconds instead of 15
    uint32_t wsHeartbeatTimeout = 120000;  // ms - 2 minutes timeout instead of 45 seconds
    void wsMarkSeen(uint32_t id) {
        for (auto &c : wsClients) { if (c.id == id) { c.lastSeen = millis(); return; } }
        wsClients.push_back({id, millis()});
    }
    void wsRemove(uint32_t id) {
        wsClients.erase(std::remove_if(wsClients.begin(), wsClients.end(), [id](const WsClientInfo &c){ return c.id == id; }), wsClients.end());
    }
    void wsHeartbeatMaintenance() {
        if (!wsEnabled || !ws) return;
        const unsigned long now = millis();
        if (now - wsLastHeartbeat >= wsHeartbeatInterval) {
            wsLastHeartbeat = now;
            ws->textAll("__ping"); // application-level ping
        }
        // Drop stale clients
        for (const auto &c : wsClients) {
            if (now - c.lastSeen > wsHeartbeatTimeout) {
                if (auto *cl = ws->client(c.id)) cl->close();
            }
        }
        // cleanup removed
        wsClients.erase(std::remove_if(wsClients.begin(), wsClients.end(), [this](const WsClientInfo &c){ return ws->client(c.id) == nullptr; }), wsClients.end());
        ws->cleanupClients();
    }
#endif

public:
    ConfigManagerClass()
    {
        appVersion = String(CONFIGMANAGER_VERSION);
        webManager.setCallbacks(
            [this]()
            { return toJSON(true); }, // config JSON - include secrets for web interface
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
                return updateSetting(group, key, value);  // Save to flash
            },
            [this](const String &group, const String &key, const String &value) -> bool
            {
                return applySetting(group, key, value);   // Memory only
            });
    }

    // Runtime manager accessor (for GUI/runtime integrations)
    ConfigManagerRuntime& getRuntime() { return runtimeManager; }

    // Settings management
    BaseSetting *findSetting(const String &category, const String &key)
    {
        for (auto *setting : settings)
        {
            if (String(setting->getCategory()) == category && String(setting->getDisplayName()) == key)
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

    // Debug method to check registered settings count
    size_t getSettingsCount() const { return settings.size(); }

    void debugPrintSettings() const
    {
        CM_LOG("[DEBUG] Total registered settings: %d", settings.size());
        for (size_t i = 0; i < settings.size(); i++) {
            const auto* s = settings[i];
            CM_LOG("[DEBUG] Setting %d: name='%s', category='%s', key='%s', visible=%s",
                   i, s->getDisplayName(), s->getCategory(), s->getKey(),
                   s->isVisible() ? "true" : "false");
        }
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

        // After loading persisted settings on boot, propagate important runtime-affecting values
        // so subsystems (like OTA) reflect saved configuration immediately.
        auto containsNoCase = [](const String &hay, const char *needle) {
            String h = hay; h.toLowerCase();
            String n = String(needle); n.toLowerCase();
            return h.indexOf(n) >= 0;
        };

        for (auto *s : settings) {
            String cat = s->getCategory();
            String key = s->getDisplayName();

            // System OTA password at boot
            if (containsNoCase(cat, "system") && containsNoCase(key, "ota") && containsNoCase(key, "pass")) {
                // Password values are masked in JSON; read the actual value directly.
                if (s->getType() == SettingType::STRING) {
                    auto *cs = static_cast<Config<String>*>(s);
                    const String pwd = cs->get();
                    if (pwd.length() > 0) {
                        otaManager.setPassword(pwd);
                        CM_LOG("[DEBUG] OTA password applied from persisted settings at boot (len=%d)", (int)pwd.length());
                    }
                }
            }
        }
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

    // Apply setting to memory only (temporary, lost after reboot)
    bool applySetting(const String &category, const String &key, const String &value)
    {
        CM_LOG("[DEBUG] applySetting called (memory only): category='%s', key='%s', value='%s'",
               category.c_str(), key.c_str(), value.c_str());

        BaseSetting *setting = findSetting(category, key);
        if (!setting)
        {
            CM_LOG("[ERROR] Setting not found: %s.%s", category.c_str(), key.c_str());
            return false;
        }

        CM_LOG("[DEBUG] Found setting: %s.%s (storage key: %s)",
               setting->getCategory(), setting->getDisplayName(), setting->getName());

        DynamicJsonDocument doc(256);

        // Convert string value to appropriate JSON type based on setting type
        if (value == "true") {
            doc.set(true);
        } else if (value == "false") {
            doc.set(false);
        } else {
            // Try to parse as number first, then fallback to string
            char* endPtr;
            long longVal = strtol(value.c_str(), &endPtr, 10);
            if (*endPtr == '\0') {
                // Successfully parsed as integer
                doc.set((int)longVal);
            } else {
                // Try as float
                float floatVal = strtof(value.c_str(), &endPtr);
                if (*endPtr == '\0') {
                    // Successfully parsed as float
                    doc.set(floatVal);
                } else {
                    // Use as string
                    doc.set(value);
                }
            }
        }

        bool result = setting->fromJSON(doc.as<JsonVariant>());

        // Side-effects: keep runtime subsystems in sync with specific settings
        if (result) {
            // Heuristic: treat keys containing both "ota" and "pass" (case-insensitive) as OTA password
            auto containsNoCase = [](const String &hay, const char *needle) {
                String h = hay; h.toLowerCase();
                String n = String(needle); n.toLowerCase();
                return h.indexOf(n) >= 0;
            };
            if (containsNoCase(category, "system") && containsNoCase(key, "ota") && containsNoCase(key, "pass")) {
                // Apply immediately to OTA manager; this affects both HTTP OTA (/ota_update) and ArduinoOTA if initialized
                otaManager.setPassword(value);
                CM_LOG("[DEBUG] OTA password (memory) updated via setting '%s.%s' (len=%d)", category.c_str(), key.c_str(), (int)value.length());
            }
        }

        CM_LOG("[DEBUG] Setting apply result (memory only): %s", result ? "SUCCESS" : "FAILED");
        return result;
    }

    // Update setting and save to flash (persistent)
    bool updateSetting(const String &category, const String &key, const String &value)
    {
        CM_LOG("[DEBUG] updateSetting called (save to flash): category='%s', key='%s', value='%s'",
               category.c_str(), key.c_str(), value.c_str());

        BaseSetting *setting = findSetting(category, key);
        if (!setting)
        {
            CM_LOG("[ERROR] Setting not found: %s.%s", category.c_str(), key.c_str());
            return false;
        }

        CM_LOG("[DEBUG] Found setting: %s.%s (storage key: %s)",
               setting->getCategory(), setting->getDisplayName(), setting->getName());

        DynamicJsonDocument doc(256);

        // Convert string value to appropriate JSON type based on setting type
        if (value == "true") {
            CM_LOG("[DEBUG] Converting string 'true' to boolean true");
            doc.set(true);
        } else if (value == "false") {
            CM_LOG("[DEBUG] Converting string 'false' to boolean false");
            doc.set(false);
        } else {
            // Try to parse as number first, then fallback to string
            char* endPtr;
            long longVal = strtol(value.c_str(), &endPtr, 10);
            if (*endPtr == '\0') {
                // Successfully parsed as integer
                CM_LOG("[DEBUG] Converting string '%s' to integer %ld", value.c_str(), longVal);
                doc.set((int)longVal);
            } else {
                // Try as float
                float floatVal = strtof(value.c_str(), &endPtr);
                if (*endPtr == '\0') {
                    // Successfully parsed as float
                    CM_LOG("[DEBUG] Converting string '%s' to float %.2f", value.c_str(), floatVal);
                    doc.set(floatVal);
                } else {
                    // Use as string
                    CM_LOG("[DEBUG] Using value '%s' as string", value.c_str());
                    doc.set(value);
                }
            }
        }

        CM_LOG("[DEBUG] JSON document created. Calling fromJSON...");
        bool result = setting->fromJSON(doc.as<JsonVariant>());

        if (result) {
            // Save the updated setting to flash storage immediately
            CM_LOG("[DEBUG] Saving setting to flash storage");

            // Save only this specific setting to flash
            if (!prefs.begin("ConfigManager", false))
            {
                CM_LOG("[ERROR] Failed to open preferences for saving");
                return false;
            }
            setting->save(prefs);
            prefs.end();

            CM_LOG("[DEBUG] Setting saved to flash successfully");

            // Side-effects: keep runtime subsystems in sync with specific settings
            // Heuristic: treat keys containing both "ota" and "pass" (case-insensitive) as OTA password
            auto containsNoCase = [](const String &hay, const char *needle) {
                String h = hay; h.toLowerCase();
                String n = String(needle); n.toLowerCase();
                return h.indexOf(n) >= 0;
            };
            if (containsNoCase(category, "system") && containsNoCase(key, "ota") && containsNoCase(key, "pass")) {
                otaManager.setPassword(value);
                CM_LOG("[DEBUG] OTA password (persisted) updated via setting '%s.%s' (len=%d)", category.c_str(), key.c_str(), (int)value.length());
            }
        }

        CM_LOG("[DEBUG] Setting update result: %s", result ? "SUCCESS" : "FAILED");
        return result;
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

    void setVersion(const String &version)
    {
        appVersion = version;
        CM_LOG("[I] App version set: %s", version.c_str());
    }

    const String &getVersion() const { return appVersion; }

    // Settings security management
    void setSettingsPassword(const String &password)
    {
        webManager.setSettingsPassword(password);
        CM_LOG("[I] Settings password configured");
    }
    
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

    void startWebServer(const IPAddress &staticIP, const IPAddress &gateway, const IPAddress &subnet, const String &ssid, const String &password, const IPAddress &dns1 = IPAddress(), const IPAddress &dns2 = IPAddress())
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
        wifiManager.startConnection(staticIP, gateway, subnet, ssid, password, dns1, dns2);

        webManager.begin(this);
        otaManager.begin(this);
        runtimeManager.begin(this);
    }

    void startAccessPoint(const String &apSSID = "", const String &apPassword = "")
    {
        String ssid = apSSID.isEmpty() ? (appName + "_AP") : apSSID;
        CM_LOG("[I] Starting Access Point: %s", ssid.c_str());

        wifiManager.begin();
        wifiManager.setCallbacks(
            []()
            {
                // Propagate STA connection events even when AP mode was forced
                extern void onWiFiConnected();
                onWiFiConnected();
            },
            []()
            {
                extern void onWiFiDisconnected();
                onWiFiDisconnected();
            },
            []()
            {
                extern void onWiFiAPMode();
                onWiFiAPMode();
            });
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
        customCss = css;
        customCssLen = len;
        CM_LOG("[I] Custom CSS registered (len=%u)", (unsigned)len);
    }

    const char* getCustomCss() const { return customCss; }
    size_t getCustomCssLen() const { return customCss ? (customCssLen ? customCssLen : strlen(customCss)) : 0; }

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
        // Create runtime field using addRuntimeMeta
        RuntimeFieldMeta meta;
        meta.group = category;
        meta.key = key;
        meta.label = name;
        meta.unit = unit;
        meta.alarmMin = min;
        meta.alarmMax = max;
        meta.hasAlarm = true;  // Since min/max are provided, assume alarm functionality is wanted
        runtimeManager.addRuntimeMeta(meta);
    }

    // Interactive runtime controls (delegated to RuntimeManager)
    void defineRuntimeButton(const String& group, const String& key, const String& label,
                           std::function<void()> onPress, const String& card = String(), int order = 100) {
        runtimeManager.defineRuntimeButton(group, key, label, onPress, card, order);
    }

    void defineRuntimeCheckbox(const String& group, const String& key, const String& label,
                             std::function<bool()> getter, std::function<void(bool)> setter,
                             const String& card = String(), int order = 100) {
        runtimeManager.defineRuntimeCheckbox(group, key, label, getter, setter, card, order);
    }

    void defineRuntimeStateButton(const String& group, const String& key, const String& label,
                                std::function<bool()> getter, std::function<void(bool)> setter,
                                bool initState = false, const String& card = String(), int order = 100) {
        runtimeManager.defineRuntimeStateButton(group, key, label, getter, setter, initState, card, order);
    }

    void defineRuntimeIntSlider(const String& group, const String& key, const String& label,
                              int minValue, int maxValue, int initValue,
                              std::function<int()> getter, std::function<void(int)> setter,
                              const String& unit = String(), const String& card = String(), int order = 100) {
        runtimeManager.defineRuntimeIntSlider(group, key, label, minValue, maxValue, initValue, getter, setter, unit, card, order);
    }

    void defineRuntimeFloatSlider(const String& group, const String& key, const String& label,
                                float minValue, float maxValue, float initValue, int precision,
                                std::function<float()> getter, std::function<void(float)> setter,
                                const String& unit = String(), const String& card = String(), int order = 100) {
        runtimeManager.defineRuntimeFloatSlider(group, key, label, minValue, maxValue, initValue, precision, getter, setter, unit, card, order);
    }

    void defineRuntimeStateButton(const String &category, const String &key, const String &name, std::function<bool()> getter, std::function<void(bool)> setter)
    {
        runtimeManager.defineRuntimeStateButton(category, key, name, getter, setter);
    }

    void defineRuntimeBool(const String &category, const String &key, const String &name, bool defaultValue, int order = 0)
    {
        // Create boolean runtime field using addRuntimeMeta
        RuntimeFieldMeta meta;
        meta.group = category;
        meta.key = key;
        meta.label = name;
        meta.isBool = true;
        meta.order = order;
        runtimeManager.addRuntimeMeta(meta);
    }

    void defineRuntimeAlarm(const String &category, const String &key, const String &name, std::function<bool()> condition, std::function<void()> onTrigger = nullptr, std::function<void()> onClear = nullptr)
    {
        String alarmName = category + "." + key;
        if (onTrigger || onClear) {
            runtimeManager.addRuntimeAlarm(alarmName, condition, onTrigger, onClear);
        } else {
            runtimeManager.addRuntimeAlarm(alarmName, condition);
        }
    }

    void defineRuntimeFloatSlider(const String &category, const String &key, const String &name, float min, float max, float &variable, int precision = 1, std::function<void()> onChange = nullptr)
    {
        // Create getter and setter for the variable reference
        auto getter = [&variable]() -> float { return variable; };
        auto setter = [&variable, onChange](float value) { 
            variable = value; 
            if (onChange) onChange(); 
        };
        
        runtimeManager.defineRuntimeFloatSlider(category, key, name, min, max, variable, precision, getter, setter);
    }

    void handleRuntimeAlarms()
    {
        runtimeManager.updateAlarms();
    }

    bool isOTAInitialized()
    {
        return otaManager.isInitialized();
    }

    void stopOTA()
    {
        // Note: stop() method not implemented in OTAManager
        CM_LOG("[W] stopOTA not implemented in OTAManager");
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
            // Do not pre-filter dynamic visibility here; include all web-visible
            // settings and let the UI honor the resolved showIf boolean.
            if (!s->shouldShowInWeb())
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
                    catObj["categoryPretty"] = prettyName; // Used by frontend Category.vue
                }
            }

            if (includeSecrets || !s->isSecret())
            {
                if (s->isSecret()) {
                    CM_LOG("[DEBUG] Including secret field: %s.%s (includeSecrets=%s)",
                           s->getCategory(), s->getDisplayName(),
                           includeSecrets ? "true" : "false");
                }
                s->toJSON(catObj);
            }
            else
            {
                CM_LOG("[DEBUG] Skipping secret field: %s.%s (includeSecrets=%s)",
                       s->getCategory(), s->getDisplayName(),
                       includeSecrets ? "true" : "false");
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
        // Wire up reboot callback for web OTA uploads
        otaManager.setCallbacks(
            []() { 
                ESP.restart(); 
            },
            [](const char* msg) { 
                ConfigManagerClass::log_message(msg); 
            }
        );
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
        // Run manual heartbeat maintenance
        wsHeartbeatMaintenance();
    }

    void enableWebSocketPush(uint32_t intervalMs = 2000)
    {
        if (!wsInitialized)
        {
            ws = new AsyncWebSocket("/ws");
            // Note: Heartbeat API not available in this ESPAsyncWebServer fork. If needed,
            // we can implement periodic ping/pong via a timer and drop stale clients.
            ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
                        {
                switch (type) {
                case WS_EVT_CONNECT:
                    CM_LOG_VERBOSE("[WS] Client connect %u", client->id());
                    wsMarkSeen(client->id());
                    if (pushOnConnect) handleWebsocketPush();
                    break;
                case WS_EVT_DISCONNECT:
                    CM_LOG_VERBOSE("[WS] Client disconnect %u", client->id());
                    wsRemove(client->id());
                    break;
                case WS_EVT_ERROR:
                    CM_LOG_VERBOSE("[WS] Error on client %u", client->id());
                    break;
                case WS_EVT_PONG:
                    CM_LOG_VERBOSE("[WS] Pong from %u", client->id());
                    wsMarkSeen(client->id());
                    break;
                case WS_EVT_DATA:
                {
                    // check for application-level pong
                    if (arg && data && len) {
                        AwsFrameInfo *info = reinterpret_cast<AwsFrameInfo*>(arg);
                        if (info && info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                            // Small fixed buffer for compare
                            char buf[16];
                            size_t n = len < sizeof(buf)-1 ? len : sizeof(buf)-1;
                            memcpy(buf, data, n);
                            buf[n] = '\0';
                            if (strcmp(buf, "__pong") == 0) {
                                wsMarkSeen(client->id());
                            }
                        }
                    }
                    break;
                }
                    break;
                }
            });
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

    // Smart WiFi Roaming configuration - convenience methods
    void enableSmartRoaming(bool enable = true) { wifiManager.enableSmartRoaming(enable); }
    void setRoamingThreshold(int thresholdDbm = -75) { wifiManager.setRoamingThreshold(thresholdDbm); }
    void setRoamingCooldown(unsigned long cooldownSeconds = 120) { wifiManager.setRoamingCooldown(cooldownSeconds); }
    void setRoamingImprovement(int improvementDbm = 10) { wifiManager.setRoamingImprovement(improvementDbm); }
    bool isSmartRoamingEnabled() const { return wifiManager.isSmartRoamingEnabled(); }

    // MAC Address Filtering and Priority - convenience methods
    void setWifiAPMacFilter(const String& macAddress) { wifiManager.setWifiAPMacFilter(macAddress); }
    void setWifiAPMacPriority(const String& macAddress) { wifiManager.setWifiAPMacPriority(macAddress); }
    void clearMacFilter() { wifiManager.clearMacFilter(); }
    void clearMacPriority() { wifiManager.clearMacPriority(); }
    bool isMacFilterEnabled() const { return wifiManager.isMacFilterEnabled(); }
    bool isMacPriorityEnabled() const { return wifiManager.isMacPriorityEnabled(); }
    String getFilterMac() const { return wifiManager.getFilterMac(); }
    String getPriorityMac() const { return wifiManager.getPriorityMac(); }

public:
    static LogCallback logger;
    WebHTML webhtml;
};

extern ConfigManagerClass ConfigManager;