#pragma once

#include <Arduino.h>
#include <string>
#include <string_view>
#include <optional>
#include <Preferences.h>
#include <vector>
#include <functional>
#include <memory>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <type_traits>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <exception>
#include <stdexcept>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <set>
#include <map>
#include <utility>

#include "ConfigManagerConfig.h"

#include "io/ioDefinitions.h"

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

#define CONFIGMANAGER_VERSION "3.3.0" // Synced to library.json

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
    std::function<void(T)> callback = nullptr;       // Value change callback
    std::function<bool()> showIf = nullptr; // Conditional visibility
    const char *categoryPretty = nullptr;   // Optional pretty name for category/card

    // Optional card grouping within a category (Settings UI)
    const char *card = nullptr;             // Optional card key within the category
    const char *cardPretty = nullptr;       // Optional pretty name for the card
    int cardOrder = 100;                    // Optional sort order for the card
};

// Server abstraction
extern AsyncWebServer server;
class ConfigManagerClass;

// Logging macros
#if CM_ENABLE_LOGGING
#define CM_LOG(...) ConfigManagerClass::log_message(__VA_ARGS__)
#if CM_ENABLE_VERBOSE_LOGGING
#define CM_LOG_VERBOSE(...) ConfigManagerClass::log_verbose_message(__VA_ARGS__)
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

#define CM_CORE_LOG(...) CM_LOG("[CM] " __VA_ARGS__)
#define CM_CORE_LOG_VERBOSE(...) CM_LOG_VERBOSE("[CM] " __VA_ARGS__)

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

inline uint64_t fnv1aHash64(const char *data, size_t len)
{
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < len; ++i)
    {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
        hash *= 1099511628211ull;
    }
    return hash;
}

inline String hashStringForStorage(const String &value)
{
    const size_t len = value.length();
    const uint64_t hashValue = fnv1aHash64(value.c_str(), len);
    char buffer[16];
    // Keep result under 15 characters (HEX) to satisfy Preferences key limits
    snprintf(buffer, sizeof(buffer), "%012llX", static_cast<unsigned long long>(hashValue & 0xFFFFFFFFFFFFull));
    return String(buffer);
}

struct ConfigRequestContext
{
    enum class Origin
    {
        None,
        ApplySingle,
        SaveSingle,
        ApplyAll,
        SaveAll
    };

    Origin origin = Origin::None;
    String endpoint;
    String payload;
    bool force = false;
};

inline const char *toString(ConfigRequestContext::Origin origin)
{
    switch (origin)
    {
    case ConfigRequestContext::Origin::ApplySingle:
        return "apply";
    case ConfigRequestContext::Origin::SaveSingle:
        return "save";
    case ConfigRequestContext::Origin::ApplyAll:
        return "apply_all";
    case ConfigRequestContext::Origin::SaveAll:
        return "save_all";
    default:
        return "none";
    }
}

struct GUIMessageAction
{
    String id;
    String label;
    bool primary = false;
};

enum class GUIMessageButtons
{
    Ok,
    OkCancel,
    OkCancelRetry,
    Cancel
};

using GuiMessageCallback = std::function<void()>;

enum class ValidationSeverity : uint8_t
{
    Ok,
    Warning,
    Error
};

struct IOPinValidationResult
{
    bool ok = false;
    ValidationSeverity severity = ValidationSeverity::Error;

    int pin = -1;
    cm::io::IOPinRole role = cm::io::IOPinRole::DigitalInput;

    String reason;
    String detail;

    uint32_t constraints = 0;
    uint32_t capabilities = 0;

    std::vector<int> alternatives;
};

inline const char *toString(ValidationSeverity severity)
{
    switch (severity)
    {
    case ValidationSeverity::Ok:
        return "ok";
    case ValidationSeverity::Warning:
        return "warning";
    default:
        return "error";
    }
}



// BaseSetting class (updated for new ConfigOptions structure)
class BaseSetting
{
protected:
    bool showInWeb;
    bool isPassword;
    bool modified = false;
    bool persistSetting = true;
    String storageKey;
    const char *storageKeyPtr = nullptr;
    String legacyStorageKey;
    const char *category;
    const char *displayName;
    const char *categoryPrettyName = nullptr;
    const char *cardName = nullptr;
    const char *cardPrettyName = nullptr;
    int cardOrder = 100;
    SettingType type;
    int sortOrder = 100;
    bool hasKeyLengthError = false;
    String keyLengthErrorMsg;
    inline static std::set<String> registeredStorageKeys;

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

    static bool registerStorageKey(const String &key)
    {
        return registeredStorageKeys.insert(key).second;
    }

public:
    bool hasError() const { return hasKeyLengthError; }
    const char *getError() const { return keyLengthErrorMsg.c_str(); }

    void setLogger(std::function<void(const char *)> logFunc)
    {
        logger = logFunc;
    }

    bool shouldPersist() const { return persistSetting; }
    void setPersistSettings(bool persist) { persistSetting = persist; }

    // New constructor for ConfigOptions-based initialization
    BaseSetting(const char* key, const char* name, const char* category, SettingType type,
                                bool showInWeb = true, bool isPassword = false, int sortOrder = 100, const char* categoryPretty = nullptr,
                                const char* card = nullptr, const char* cardPretty = nullptr, int cardOrder = 100)
                : displayName(name), category(category), categoryPrettyName(categoryPretty),
                    cardName(card), cardPrettyName(cardPretty), cardOrder(cardOrder), type(type),
                    showInWeb(showInWeb), isPassword(isPassword), sortOrder(sortOrder)
    {
        String legacyHint;
        if (key && key[0] != '\0') {
            legacyHint = key;
        } else {
            legacyHint = generateKeyFromNameAndCategory(name, category);
        }
        legacyStorageKey = legacyHint;
        storageKey = hashStringForStorage(legacyHint);
        storageKeyPtr = storageKey.c_str();
        if (!registerStorageKey(storageKey)) {
            hasKeyLengthError = true;
            keyLengthErrorMsg = String("Storage key collision for '") + displayName + "' (" + storageKey + ") - not persisted";
            log("[WARNING] %s", keyLengthErrorMsg.c_str());
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
    const char *getKey() const { return storageKeyPtr; }
    const char *getCategory() const { return category; }
    const char *getCategoryPretty() const { return categoryPrettyName ? categoryPrettyName : category; }
    const char *getCard() const { return cardName; }
    const char *getCardPretty() const { return cardPrettyName ? cardPrettyName : cardName; }
    int getCardOrder() const { return cardOrder; }
    const char *getName() const { return storageKeyPtr; }
    const char *getLegacyKey() const { return legacyStorageKey.c_str(); }
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
                     opts.showInWeb, opts.isPassword, opts.sortOrder, opts.categoryPretty,
                     opts.card, opts.cardPretty, opts.cardOrder),
          value(opts.defaultValue), defaultValue(opts.defaultValue)
    {
        showIfFunc = opts.showIf;
        if (opts.callback)
        {
            callback = opts.callback;
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

        const char* storageKey = getKey();
        const char* legacyKey = getLegacyKey();
        bool keyExists = prefs.isKey(storageKey);
        logVerbose("[PREFS] Setting %s.%s exists? %s", getCategory(), getDisplayName(), keyExists ? "true" : "false");

        auto readFrom = [&](const char* sourceKey) -> T {
            if constexpr (std::is_same_v<T, String>)
            {
                return prefs.getString(sourceKey, defaultValue);
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                return prefs.getBool(sourceKey, defaultValue);
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                return prefs.getInt(sourceKey, defaultValue);
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                return prefs.getFloat(sourceKey, defaultValue);
            }
        };

        auto persistValue = [&](const char* targetKey, const char* actionLabel) {
            if constexpr (std::is_same_v<T, String>)
            {
                prefs.putString(targetKey, value);
                if (isPassword)
                {
                    log("[PREFS] %s %s.%s = '***' (hidden)", actionLabel, getCategory(), getDisplayName());
                }
                else
                {
                    log("[PREFS] %s %s.%s = '%s'", actionLabel, getCategory(), getDisplayName(), value.c_str());
                }
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                prefs.putBool(targetKey, value);
                log("[PREFS] %s %s.%s = %s", actionLabel, getCategory(), getDisplayName(), value ? "true" : "false");
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                prefs.putInt(targetKey, value);
                log("[PREFS] %s %s.%s = %d", actionLabel, getCategory(), getDisplayName(), value);
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                prefs.putFloat(targetKey, value);
                log("[PREFS] %s %s.%s = %.2f", actionLabel, getCategory(), getDisplayName(), value);
            }
        };

        if (!keyExists)
        {
            bool migrated = false;
            if (legacyKey && legacyKey[0] != '\0' && strcmp(legacyKey, storageKey) != 0 && prefs.isKey(legacyKey))
            {
                value = readFrom(legacyKey);
                persistValue(storageKey, "Migrated");
                log("[PREFS] Migrated %s.%s from legacy key '%s'", getCategory(), getDisplayName(), legacyKey);
                migrated = true;
            }

            if (!migrated)
            {
                value = defaultValue;
                persistValue(storageKey, "Initialized (default)");
            }

            modified = false;
            return;
        }

        value = readFrom(storageKey);
        modified = false;

        if constexpr (std::is_same_v<T, String>)
        {
            if (isPassword) {
                logVerbose("[PREFS] Loaded %s.%s = '***' (hidden)", getCategory(), getDisplayName());
            } else {
                logVerbose("[PREFS] Loaded %s.%s = '%s'", getCategory(), getDisplayName(), value.c_str());
            }
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            logVerbose("[PREFS] Loaded %s.%s = %s", getCategory(), getDisplayName(), value ? "true" : "false");
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            logVerbose("[PREFS] Loaded %s.%s = %d", getCategory(), getDisplayName(), value);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            logVerbose("[PREFS] Loaded %s.%s = %.2f", getCategory(), getDisplayName(), value);
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
                logVerbose("[PREFS] Saved %s.%s = '***' (hidden)", getCategory(), getDisplayName());
            } else {
                logVerbose("[PREFS] Saved %s.%s = '%s'", getCategory(), getDisplayName(), value.c_str());
            }
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            prefs.putBool(getKey(), value);
            logVerbose("[PREFS] Saved %s.%s = %s", getCategory(), getDisplayName(), value ? "true" : "false");
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            prefs.putInt(getKey(), value);
            logVerbose("[PREFS] Saved %s.%s = %d", getCategory(), getDisplayName(), value);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            prefs.putFloat(getKey(), value);
            logVerbose("[PREFS] Saved %s.%s = %.2f", getCategory(), getDisplayName(), value);
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
        const char* jsonKey = (getCard() && getCard()[0]) ? getKey() : getDisplayName();
        JsonObject settingObj = obj.createNestedObject(jsonKey);

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
        settingObj["key"] = getKey();
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

    template<typename T>
    class SettingBuilder
    {
    public:
        SettingBuilder(ConfigManagerClass &owner, const char *key)
            : manager(owner)
        {
            opts.key = key;
            opts.name = key ? key : ConfigManagerClass::DEFAULT_LAYOUT_NAME;
            opts.category = ConfigManagerClass::DEFAULT_LAYOUT_NAME;
            opts.defaultValue = T{};
        }

        SettingBuilder &key(const char *customKey)
        {
            opts.key = customKey;
            return *this;
        }

        SettingBuilder &name(const char *displayName)
        {
            opts.name = displayName;
            return *this;
        }

        SettingBuilder &category(const char *categoryName)
        {
            opts.category = categoryName;
            return *this;
        }

        SettingBuilder &categoryPretty(const char *prettyName)
        {
            opts.categoryPretty = prettyName;
            return *this;
        }

        SettingBuilder &card(const char *cardName)
        {
            opts.card = cardName;
            return *this;
        }

        SettingBuilder &cardPretty(const char *prettyName)
        {
            opts.cardPretty = prettyName;
            return *this;
        }

        SettingBuilder &cardOrder(int order)
        {
            opts.cardOrder = order;
            return *this;
        }

        SettingBuilder &sortOrder(int order)
        {
            opts.sortOrder = order;
            return *this;
        }

        SettingBuilder &defaultValue(const T &value)
        {
            opts.defaultValue = value;
            return *this;
        }

        SettingBuilder &showInWeb(bool value)
        {
            opts.showInWeb = value;
            return *this;
        }

        SettingBuilder &password(bool value)
        {
            opts.isPassword = value;
            return *this;
        }

        SettingBuilder &callback(std::function<void(T)> cb)
        {
            opts.callback = cb;
            return *this;
        }

        SettingBuilder &showIf(std::function<bool()> predicate)
        {
            opts.showIf = predicate;
            return *this;
        }

        SettingBuilder &persist(bool value)
        {
            persistFlag = value;
            return *this;
        }

        SettingBuilder &ioPinRole(cm::io::IOPinRole role)
        {
            isIOPinSetting = true;
            ioPinRoleValue = role;
            return *this;
        }

        Config<T> &build()
        {
            auto settingPtr = std::make_unique<Config<T>>(opts);
            settingPtr->setPersistSettings(persistFlag);
            BaseSetting *raw = manager.addSetting(std::move(settingPtr));
            if (!raw)
            {
                std::string message = "Failed to register setting ";
                message += opts.name ? opts.name : "(unnamed)";
                throw std::runtime_error(message);
            }
            if (isIOPinSetting && raw->getKey())
            {
                manager.registerIOPinSetting(raw->getKey(), ioPinRoleValue);
            }
            return *static_cast<Config<T> *>(raw);
        }

    private:
        ConfigOptions<T> opts;
        ConfigManagerClass &manager;
        bool persistFlag = true;
        bool isIOPinSetting = false;
        cm::io::IOPinRole ioPinRoleValue = cm::io::IOPinRole::DigitalInput;
    };

    static constexpr const char *DEFAULT_LAYOUT_NAME = "Default";
    static constexpr int DEFAULT_LAYOUT_ORDER = 100;
    static constexpr const char *DEFAULT_LIVE_CARD_NAME = "Live Values";

private:
    Preferences prefs;
    std::vector<BaseSetting*> settings;
    std::vector<std::unique_ptr<BaseSetting>> ownedSettings;
    String appName;
    String appTitle;
    String appVersion;

    cm::io::GUIMode guiMode = cm::io::GUIMode::Esp32;
    std::unique_ptr<cm::io::IOPinRules> pinRules = cm::io::createPinRulesForMode(guiMode);
    std::map<String, cm::io::IOPinRole> ioPinRoles;
    std::vector<ConfigRequestContext> requestContextStack;
    ConfigRequestContext fallbackRequestContext;
    uint32_t guiMessageCounter = 0;
    std::map<String, std::map<String, GuiMessageCallback>> guiMessageCallbacks;

    void sendGUIMessage(const char *type,
                        const String &title,
                        const String &message,
                        const String &details,
                        const std::vector<GUIMessageAction> &actions,
                        std::function<void(JsonObject &)> contextBuilder)
    {
        DynamicJsonDocument doc(1024);
        JsonObject root = doc.to<JsonObject>();
        root["type"] = type;
        root["title"] = title;
        root["message"] = message;
        root["details"] = details;
        root["mode"] = cm::io::toString(guiMode);
        root["origin"] = toString(getCurrentRequestContext().origin);
        JsonArray actionArray = root.createNestedArray("actions");
        for (const auto &act : actions)
        {
            JsonObject item = actionArray.createNestedObject();
            item["id"] = act.id;
            item["label"] = act.label;
            item["primary"] = act.primary;
        }
        if (contextBuilder)
        {
            JsonObject contextObj = root.createNestedObject("context");
            contextBuilder(contextObj);
        }
        String payload;
        serializeJson(doc, payload);
        if (!sendWebSocketPayload(payload))
        {
            CM_CORE_LOG("[CM] WebSocket payload not delivered: %s", payload.c_str());
        }
    }

    void sendSeverityMessage(const char *type,
                             const String &title,
                             const String &message,
                             const String &details,
                             GUIMessageButtons buttons,
                             GuiMessageCallback cbOk,
                             GuiMessageCallback cbCancel,
                             GuiMessageCallback cbRetry,
                             std::function<void(JsonObject &)> contextBuilder)
    {
        const String messageId = allocateGuiMessageId();
        const std::vector<GUIMessageAction> actions = buildGuiMessageActions(buttons);
        registerGuiMessageCallbacks(messageId, actions, cbOk, cbCancel, cbRetry);
        sendGUIMessage(type, title, message, details, actions,
                       [this, messageId, contextBuilder](JsonObject &obj)
                       {
                           obj["guiActionEndpoint"] = "/gui/action";
                           obj["guiMessageId"] = messageId;
                           if (contextBuilder)
                           {
                               contextBuilder(obj);
                           }
                       });
    }

    std::vector<GUIMessageAction> buildGuiMessageActions(GUIMessageButtons buttons) const
    {
        std::vector<GUIMessageAction> actions;
        switch (buttons)
        {
        case GUIMessageButtons::Ok:
            actions.push_back({"ok", "Ok", true});
            break;
        case GUIMessageButtons::OkCancel:
            actions.push_back({"ok", "Ok", true});
            actions.push_back({"cancel", "Cancel", false});
            break;
        case GUIMessageButtons::OkCancelRetry:
            actions.push_back({"ok", "Ok", true});
            actions.push_back({"cancel", "Cancel", false});
            actions.push_back({"retry", "Retry", false});
            break;
        case GUIMessageButtons::Cancel:
            actions.push_back({"cancel", "Cancel", true});
            break;
        }
        return actions;
    }

    void registerGuiMessageCallbacks(const String &messageId,
                                     const std::vector<GUIMessageAction> &actions,
                                     const GuiMessageCallback &cbOk,
                                     const GuiMessageCallback &cbCancel,
                                     const GuiMessageCallback &cbRetry)
    {
        auto &callbacks = guiMessageCallbacks[messageId];
        for (const auto &action : actions)
        {
            if (action.id == "ok" && cbOk)
            {
                callbacks[action.id] = cbOk;
            }
            else if (action.id == "cancel" && cbCancel)
            {
                callbacks[action.id] = cbCancel;
            }
            else if (action.id == "retry" && cbRetry)
            {
                callbacks[action.id] = cbRetry;
            }
        }
    }

    String allocateGuiMessageId()
    {
        ++guiMessageCounter;
        return String("gui_msg_") + String(guiMessageCounter, DEC);
    }

    IOPinValidationResult validateIOPinSetting(const String &key, const String &value) const
    {
        IOPinValidationResult result;
        if (key.isEmpty())
            return result;

        auto it = ioPinRoles.find(key);
        if (it == ioPinRoles.end())
            return result;

        result.role = it->second;
        char *endPtr = nullptr;
        long parsed = strtol(value.c_str(), &endPtr, 10);
        if (endPtr == value.c_str() || *endPtr != '\0')
        {
            result.reason = "Pin value is not a number";
            result.detail = String("Pin value for '") + key + "' must be numeric";
            result.severity = ValidationSeverity::Error;
            return result;
        }

        result.pin = static_cast<int>(parsed);
        if (result.pin < 0)
        {
            result.reason = "Pin value must be positive";
            result.detail = String("Pin value for '") + key + "' cannot be negative";
            result.severity = ValidationSeverity::Error;
            return result;
        }

        if (!pinRules)
        {
            result.reason = "Pin metadata unavailable";
            result.detail = String("Pin validation cannot run before GUI mode rules are loaded for ") + cm::io::toString(guiMode);
            result.severity = ValidationSeverity::Warning;
            return result;
        }

        const cm::io::PinInfo info = pinRules->getPinInfo(result.pin);
        result.capabilities = info.capabilities;
        result.constraints = info.constraints;

        if (!info.exists)
        {
            result.reason = "Invalid pin";
            result.detail = String("Pin ") + String(result.pin) + " does not exist on this board configuration";
            result.severity = ValidationSeverity::Error;
            return result;
        }

        if (!hasRoleCapability(result.role, info.capabilities))
        {
            result.reason = String("Pin not supported for ") + cm::io::toString(result.role);
            result.detail = String("Pin ") + String(result.pin) + " lacks the required capability for mode " + cm::io::toString(guiMode);
            result.severity = ValidationSeverity::Error;
            return result;
        }

        const String constraintHint = pinRules ? pinRules->describeConstraints(info.constraints) : String();
        if (!constraintHint.isEmpty())
        {
            result.reason = "Pin has hardware caveats";
            result.detail = String("Pin ") + String(result.pin) + " is usable but " + constraintHint;
            if (!result.detail.endsWith("."))
            {
                result.detail += ".";
            }
            result.severity = ValidationSeverity::Warning;
            return result;
        }

        result.ok = true;
        result.severity = ValidationSeverity::Ok;
        return result;
    }

    static bool hasRoleCapability(cm::io::IOPinRole role, uint32_t capabilities)
    {
        switch (role)
        {
        case cm::io::IOPinRole::DigitalOutput:
            return cm::io::has(capabilities, cm::io::PinCapability::DigitalOut) ||
                   cm::io::has(capabilities, cm::io::PinCapability::PWMOut);
        case cm::io::IOPinRole::DigitalInput:
            return cm::io::has(capabilities, cm::io::PinCapability::DigitalIn);
        case cm::io::IOPinRole::AnalogInput:
            return cm::io::has(capabilities, cm::io::PinCapability::AnalogIn);
        case cm::io::IOPinRole::AnalogOutput:
            return cm::io::has(capabilities, cm::io::PinCapability::DACOut) ||
                   cm::io::has(capabilities, cm::io::PinCapability::PWMOut);
        }
        return false;
    }

    bool shouldBlockIOPinChange(BaseSetting *setting,
                                const String &category,
                                const String &key,
                                const String &value,
                                bool isApply)
    {
        if (!setting)
        {
            return false;
        }
        const char *storageKey = setting->getKey();
        if (!storageKey || storageKey[0] == '\0')
        {
            return false;
        }
        auto it = ioPinRoles.find(storageKey);
        if (it == ioPinRoles.end())
        {
            return false;
        }

        const IOPinValidationResult result = validateIOPinSetting(storageKey, value);
        if (result.ok || result.severity == ValidationSeverity::Ok)
        {
            return false;
        }

        const bool forced = getCurrentRequestContext().force;
        const bool shouldBlock = (result.severity == ValidationSeverity::Error) && !forced;
        handleIOPinValidationFailure(category, key, value, result, isApply);
        if (shouldBlock)
        {
            CM_CORE_LOG("[W] IO pin validation blocked %s.%s value '%s' (reason: %s)",
                        category.c_str(), key.c_str(), value.c_str(), result.reason.c_str());
        }
        else if (!result.reason.isEmpty())
        {
            CM_CORE_LOG("[W] IO pin validation warning for %s.%s: %s",
                        category.c_str(), key.c_str(), result.reason.c_str());
        }
        return shouldBlock;
    }

    void handleIOPinValidationFailure(const String &category,
                                     const String &key,
                                     const String &value,
                                     const IOPinValidationResult &result,
                                     bool isApply)
    {
        const ConfigRequestContext &ctx = getCurrentRequestContext();
        const String origin = toString(ctx.origin);
        const std::vector<GUIMessageAction> actions = {
            {"force", "Force anyway", true},
            {"cancel", "Cancel", false}
        };
        const char *messageType = (result.severity == ValidationSeverity::Warning)
                                      ? "warningMessage"
                                      : "errorMessage";

        sendGUIMessage(
            messageType,
            "Invalid pin configuration",
            result.reason,
            result.detail,
            actions,
            [&](JsonObject &obj)
            {
                obj["category"] = category;
                obj["key"] = key;
                obj["value"] = value;
                obj["pin"] = result.pin;
                obj["role"] = cm::io::toString(result.role);
                obj["mode"] = cm::io::toString(guiMode);
                obj["origin"] = origin;
                obj["severity"] = toString(result.severity);
                String constraintsHex = String(result.constraints, HEX);
                constraintsHex.toUpperCase();
                obj["constraints"] = constraintsHex;
                String capabilitiesHex = String(result.capabilities, HEX);
                capabilitiesHex.toUpperCase();
                obj["capabilities"] = capabilitiesHex;
                if (!result.alternatives.empty())
                {
                    JsonArray alt = obj.createNestedArray("alternatives");
                    for (int altPin : result.alternatives)
                    {
                        alt.add(altPin);
                    }
                }
                obj["isApply"] = isApply;
                if (!ctx.endpoint.isEmpty())
                {
                    obj["endpoint"] = ctx.endpoint;
                }
                obj["forceParam"] = "force=1";
                if (!ctx.payload.isEmpty())
                {
                    obj["payload"] = ctx.payload;
                }
            });
    }

    bool sendWebSocketPayload(const String &payload)
        {
    #if CM_ENABLE_WS_PUSH
        return sendWebSocketText(payload);
    #else
        (void)payload;
        return false;
    #endif
        }

        // Modular components
    ConfigManagerWiFi wifiManager;
    ConfigManagerWeb webManager;
    ConfigManagerOTA otaManager;
    ConfigManagerRuntime runtimeManager;

    // Optional user-provided global CSS to inject into the Web UI
    const char* customCss = nullptr;
    size_t customCssLen = 0; // 0 -> treat as null-terminated string

    bool guiLoggingEnabled = false;

    struct LayoutGroup
    {
        String name;
        int order = DEFAULT_LAYOUT_ORDER;

    };

    struct CategoryLayoutOverride
    {
        String page;
        String card;
        String group;
        int order = DEFAULT_LAYOUT_ORDER;
    };

    struct LayoutCard
    {
        String name;
        int order = DEFAULT_LAYOUT_ORDER;
        std::vector<LayoutGroup> groups;
    };

    struct LayoutPage
    {
        String name;
        int order = DEFAULT_LAYOUT_ORDER;
        std::vector<LayoutCard> cards;
    };

    std::vector<LayoutPage> settingsPages;
    std::vector<LayoutPage> livePages;
    std::set<String> layoutWarnings;
    std::map<String, CategoryLayoutOverride> categoryLayoutOverrides;

    LayoutPage *findLayoutPage(std::vector<LayoutPage> &pages, const String &normalized);
    LayoutCard *findLayoutCard(LayoutPage &page, const String &normalized);
    LayoutGroup *findLayoutGroup(LayoutCard &card, const String &normalized);
    LayoutPage &ensureLayoutPage(std::vector<LayoutPage> &pages, const char *name, int order, bool warnOnCreate);
    LayoutCard &ensureLayoutCard(LayoutPage &page, const char *name, int order, const String &fallbackName, bool warnOnCreate);
    LayoutGroup &ensureLayoutGroup(LayoutCard &card, const char *name, int order, const String &fallbackName, bool warnOnCreate);
    String normalizeLayoutName(const String &value) const;
    void logLayoutWarningOnce(const String &key, const String &message);
public:
    void setCategoryLayoutOverride(const char *category, const char *page, const char *card, const char *group, int order);
    void registerLivePlacement(const String& liveGroup,
                               const String& key,
                               const String& label,
                               int order = DEFAULT_LAYOUT_ORDER);

    const CategoryLayoutOverride *getCategoryLayoutOverride(const char *category) const;

    // WebSocket support
#if CM_ENABLE_WS_PUSH
    AsyncWebSocket *ws = nullptr;
    bool wsInitialized = false;
    bool wsEnabled = false;
    bool pushOnConnect = true;
    uint32_t wsInterval = 2000;

    unsigned long wsLastPush = 0;
    std::function<String()> customPayloadBuilder;
    std::vector<std::function<void(AsyncWebSocketClient*)>> wsConnectCallbacks;
    std::vector<std::function<void(AsyncWebSocketClient*)>> wsDisconnectCallbacks;
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
            { for (const auto &entry : settings) entry->setDefault(); saveAll(); }, // reset callback
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
    void setGuiLoggingEnabled(bool enabled) { guiLoggingEnabled = enabled; }
    bool isGuiLoggingEnabled() const { return guiLoggingEnabled; }

    // Settings management
    BaseSetting *findSetting(const String &category, const String &key)
    {
        for (BaseSetting *setting : settings)
        {
            if (String(setting->getCategory()) == category)
            {
                if (String(setting->getKey()) == key)
                {
                    return setting;
                }
                if (const char *legacyKey = setting->getLegacyKey(); legacyKey && legacyKey[0] != '\0' && String(legacyKey) == key)
                {
                    return setting;
                }
            }
        }

        // Backwards compatibility: older clients used displayName as the JSON key.
        for (BaseSetting *setting : settings)
        {
            if (String(setting->getCategory()) == category && String(setting->getDisplayName()) == key)
            {
                return setting;
            }
        }
        return nullptr;
    }

    BaseSetting *addSetting(std::unique_ptr<BaseSetting> setting)
    {
        if (!setting || setting->hasError())
        {
            if (setting)
            {
                CM_CORE_LOG("[E] Setting error: %s", setting->getError());
            }
            return nullptr;
        }
        BaseSetting *raw = setting.get();
        raw->setLogger([](const char *msg)
                       { CM_CORE_LOG("%s", msg); });
        ownedSettings.push_back(std::move(setting));
        settings.push_back(raw);
        registerSettingPlacement(raw);
        return raw;
    }

    BaseSetting *addSetting(BaseSetting *setting)
    {
        if (!setting || setting->hasError())
        {
            if (setting)
            {
                CM_CORE_LOG("[E] Setting error: %s", setting->getError());
            }
            return nullptr;
        }
        setting->setLogger([](const char *msg)
                            { CM_CORE_LOG("%s", msg); });
        settings.push_back(setting);
        registerSettingPlacement(setting);
        return setting;
    }

    // Debug method to check registered settings count
    size_t getSettingsCount() const { return settings.size(); }

    void debugPrintSettings() const
    {
        CM_CORE_LOG("[DEBUG] Total registered settings: %d", settings.size());
        for (size_t i = 0; i < settings.size(); i++) {
            const auto* s = settings[i];
            CM_CORE_LOG("[DEBUG] Setting %d: name='%s', category='%s', key='%s', visible=%s",
                   i, s->getDisplayName(), s->getCategory(), s->getKey(),
                   s->isVisible() ? "true" : "false");
        }
    }

    // Layout registries (Settings + Live)
    void addSettingsPage(const char *pageName, int order);
    void addSettingsCard(const char *pageName, const char *cardName, int order);
    void addSettingsGroup(const char *pageName, const char *cardName, const char *groupName, int order);
    void addLivePage(const char *pageName, int order);
    void addLiveCard(const char *pageName, const char *cardName, int order);
    void addLiveGroup(const char *pageName, const char *cardName, const char *groupName, int order);
    void registerSettingPlacement(BaseSetting *setting);

    struct UiPlacement
    {
        String id;
        String page;
        String card;
        String group;
        int order = DEFAULT_LAYOUT_ORDER;
    };

    std::vector<UiPlacement> settingsPlacements;
    std::vector<UiPlacement> livePlacements;

    void addToSettings(const char *itemId, const char *pageName, int order);
    void addToSettingsGroup(const char *itemId, const char *pageName, const char *groupName, int order);
    void addToSettingsGroup(const char *itemId, const char *pageName, const char *cardName, const char *groupName, int order);
    void addToLive(const char *itemId, const char *pageName, int order);
    void addToLiveGroup(const char *itemId, const char *pageName, const char *groupName, int order);
    void addToLiveGroup(const char *itemId, const char *pageName, const char *cardName, const char *groupName, int order);

    inline SettingBuilder<bool> addSettingBool(const char *key = nullptr)
    {
        return SettingBuilder<bool>(*this, key);
    }

    inline SettingBuilder<int> addSettingInt(const char *key = nullptr)
    {
        return SettingBuilder<int>(*this, key);
    }

    inline SettingBuilder<float> addSettingFloat(const char *key = nullptr)
    {
        return SettingBuilder<float>(*this, key);
    }

    inline SettingBuilder<String> addSettingString(const char *key = nullptr)
    {
        return SettingBuilder<String>(*this, key);
    }

    void loadAll()
    {
        if (!prefs.begin("ConfigManager", false))
        {
            throw std::runtime_error("Failed to initialize preferences");
        }

        for (BaseSetting *s : settings)
        {
            if (s->shouldPersist())
            {
                s->load(prefs);
            }
        }
        prefs.end();

        // After loading persisted settings on boot, propagate important runtime-affecting values
        // so subsystems (like OTA) reflect saved configuration immediately.
        auto containsNoCase = [](const String &hay, const char *needle) {
            String h = hay; h.toLowerCase();
            String n = String(needle); n.toLowerCase();
            return h.indexOf(n) >= 0;
        };

        for (BaseSetting *s : settings) {
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
                        CM_CORE_LOG("[DEBUG] OTA password applied from persisted settings at boot (len=%d)", (int)pwd.length());
                    }
                }
            }
        }
    }

    void saveAll()
    {
        if (!prefs.begin("ConfigManager", false))
        {
            CM_CORE_LOG("[E] Failed to open preferences for writing");
            return;
        }

        for (BaseSetting *s : settings)
        {
            if (!s->shouldPersist())
            {
                continue;
            }
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
        CM_CORE_LOG("[DEBUG] applySetting called (memory only): category='%s', key='%s', value='%s'",
               category.c_str(), key.c_str(), value.c_str());

        BaseSetting *setting = findSetting(category, key);
        if (!setting)
        {
            CM_CORE_LOG("[ERROR] Setting not found: %s.%s", category.c_str(), key.c_str());
            return false;
        }

        if (shouldBlockIOPinChange(setting, category, key, value, true))
        {
            return false;
        }

        CM_CORE_LOG("[DEBUG] Found setting: %s.%s (storage key: %s)",
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
                CM_CORE_LOG("[DEBUG] OTA password (memory) updated via setting '%s.%s' (len=%d)", category.c_str(), key.c_str(), (int)value.length());
            }
        }

        CM_CORE_LOG("[DEBUG] Setting apply result (memory only): %s", result ? "SUCCESS" : "FAILED");
        return result;
    }

    // Update setting and save to flash (persistent)
    bool updateSetting(const String &category, const String &key, const String &value)
    {
        CM_CORE_LOG("[DEBUG] updateSetting called (save to flash): category='%s', key='%s', value='%s'",
               category.c_str(), key.c_str(), value.c_str());

        BaseSetting *setting = findSetting(category, key);
        if (!setting)
        {
            CM_CORE_LOG("[ERROR] Setting not found: %s.%s", category.c_str(), key.c_str());
            return false;
        }

        if (shouldBlockIOPinChange(setting, category, key, value, false))
        {
            return false;
        }

        CM_CORE_LOG("[DEBUG] Found setting: %s.%s (storage key: %s)",
               setting->getCategory(), setting->getDisplayName(), setting->getName());

        DynamicJsonDocument doc(256);

        // Convert string value to appropriate JSON type based on setting type
        if (value == "true") {
            CM_CORE_LOG("[DEBUG] Converting string 'true' to boolean true");
            doc.set(true);
        } else if (value == "false") {
            CM_CORE_LOG("[DEBUG] Converting string 'false' to boolean false");
            doc.set(false);
        } else {
            // Try to parse as number first, then fallback to string
            char* endPtr;
            long longVal = strtol(value.c_str(), &endPtr, 10);
            if (*endPtr == '\0') {
                // Successfully parsed as integer
                CM_CORE_LOG("[DEBUG] Converting string '%s' to integer %ld", value.c_str(), longVal);
                doc.set((int)longVal);
            } else {
                // Try as float
                float floatVal = strtof(value.c_str(), &endPtr);
                if (*endPtr == '\0') {
                    // Successfully parsed as float
                    CM_CORE_LOG("[DEBUG] Converting string '%s' to float %.2f", value.c_str(), floatVal);
                    doc.set(floatVal);
                } else {
                    // Use as string
                    CM_CORE_LOG("[DEBUG] Using value '%s' as string", value.c_str());
                    doc.set(value);
                }
            }
        }

        CM_CORE_LOG("[DEBUG] JSON document created. Calling fromJSON...");
        bool result = setting->fromJSON(doc.as<JsonVariant>());

        if (result) {
            if (setting->shouldPersist())
            {
                // Save the updated setting to flash storage immediately
                CM_CORE_LOG("[DEBUG] Saving setting to flash storage");

                if (!prefs.begin("ConfigManager", false))
                {
                    CM_CORE_LOG("[ERROR] Failed to open preferences for saving");
                    return false;
                }
                setting->save(prefs);
                prefs.end();

                CM_CORE_LOG("[DEBUG] Setting saved to flash successfully");
            }
            else
            {
                CM_CORE_LOG("[DEBUG] Setting %s.%s is non-persistent; skipping flash save", category.c_str(), key.c_str());
            }

            // Side-effects: keep runtime subsystems in sync with specific settings
            // Heuristic: treat keys containing both "ota" and "pass" (case-insensitive) as OTA password
            auto containsNoCase = [](const String &hay, const char *needle) {
                String h = hay; h.toLowerCase();
                String n = String(needle); n.toLowerCase();
                return h.indexOf(n) >= 0;
            };
            if (containsNoCase(category, "system") && containsNoCase(key, "ota") && containsNoCase(key, "pass")) {
                otaManager.setPassword(value);
                CM_CORE_LOG("[DEBUG] OTA password (persisted) updated via setting '%s.%s' (len=%d)", category.c_str(), key.c_str(), (int)value.length());
            }
        }

        CM_CORE_LOG("[DEBUG] Setting update result: %s", result ? "SUCCESS" : "FAILED");
        return result;
    }

    void checkSettingsForErrors()
    {
        for (BaseSetting *s : settings)
        {
            if (s->hasError())
            {
                CM_CORE_LOG("[E] Setting error: %s", s->getError());
            }
        }
    }

    void setGUIMode(cm::io::GUIMode mode)
    {
        if (guiMode == mode)
            return;
        guiMode = mode;
        pinRules = cm::io::createPinRulesForMode(guiMode);
    }

    cm::io::GUIMode getGUIMode() const
    {
        return guiMode;
    }

    void registerIOPinSetting(const char *key, cm::io::IOPinRole role)
    {
        if (!key || key[0] == '\0')
            return;
        ioPinRoles[String(key)] = role;
    }

    void pushRequestContext(const ConfigRequestContext &ctx)
    {
        requestContextStack.push_back(ctx);
    }

    void popRequestContext()
    {
        if (!requestContextStack.empty())
        {
            requestContextStack.pop_back();
        }
    }

    const ConfigRequestContext &getCurrentRequestContext() const
    {
        if (requestContextStack.empty())
        {
            return fallbackRequestContext;
        }
        return requestContextStack.back();
    }

    void sendErrorMessage(const String &title,
                          const String &message,
                          const String &details,
                          const std::vector<GUIMessageAction> &actions,
                          std::function<void(JsonObject &)> contextBuilder = nullptr)
    {
        sendGUIMessage("errorMessage", title, message, details, actions, contextBuilder);
    }

    void sendErrorMessage(const String &title,
                          const String &message,
                          const String &details,
                          GUIMessageButtons buttons,
                          GuiMessageCallback cbOk,
                          GuiMessageCallback cbCancel,
                          GuiMessageCallback cbRetry,
                          std::function<void(JsonObject &)> contextBuilder)
    {
        sendSeverityMessage("errorMessage", title, message, details, buttons, cbOk, cbCancel, cbRetry, contextBuilder);
    }

    void sendWarnMessage(const String &title,
                         const String &message,
                         const String &details,
                         GUIMessageButtons buttons,
                         GuiMessageCallback cbOk,
                         GuiMessageCallback cbCancel,
                         GuiMessageCallback cbRetry,
                         std::function<void(JsonObject &)> contextBuilder)
    {
        sendSeverityMessage("warningMessage", title, message, details, buttons, cbOk, cbCancel, cbRetry, contextBuilder);
    }

    void sendInfoMessage(const String &title,
                         const String &message,
                         const String &details,
                         const std::vector<GUIMessageAction> &actions,
                         std::function<void(JsonObject &)> contextBuilder = nullptr)
    {
        sendGUIMessage("infoMessage", title, message, details, actions, contextBuilder);
    }

    void sendInfoMessage(const String &title,
                         const String &message,
                         const String &details,
                         GUIMessageButtons buttons,
                         GuiMessageCallback cbOk,
                         GuiMessageCallback cbCancel,
                         GuiMessageCallback cbRetry,
                         std::function<void(JsonObject &)> contextBuilder)
    {
        sendSeverityMessage("infoMessage", title, message, details, buttons, cbOk, cbCancel, cbRetry, contextBuilder);
    }

    bool handleGuiAction(const String &messageId, const String &actionId)
    {
        if (messageId.isEmpty() || actionId.isEmpty())
        {
            return false;
        }
        auto msgIt = guiMessageCallbacks.find(messageId);
        if (msgIt == guiMessageCallbacks.end())
        {
            return false;
        }
        auto callbacks = msgIt->second;
        guiMessageCallbacks.erase(msgIt);
        auto cbIt = callbacks.find(actionId);
        if (cbIt != callbacks.end() && cbIt->second)
        {
            cbIt->second();
        }
        return true;
    }

    // App management
    void setAppName(const String &name)
    {
        appName = name;
        CM_CORE_LOG("[I] App name set: %s", name.c_str());
    }

    const String &getAppName() const { return appName; }

    void setAppTitle(const String &title)
    {
        appTitle = title;
        CM_CORE_LOG("[I] App title set: %s", title.c_str());
    }

    const String &getAppTitle() const { return appTitle; }

    void setVersion(const String &version)
    {
        appVersion = version;
        CM_CORE_LOG("[I] App version set: %s", version.c_str());
    }

    const String &getVersion() const { return appVersion; }

    // Settings security management
    void setSettingsPassword(const String &password)
    {
        webManager.setSettingsPassword(password);
        CM_CORE_LOG("[I] Settings password configured");
    }
    
    // WiFi management - NON-BLOCKING!
    // Convenience overload: start WiFi using persisted/registered Core WiFi settings.
    //
    // Expected settings (category: "WiFi"):
    // - WiFiSSID, WiFiPassword
    // - WiFiUseDHCP
    // - WiFiStaticIP, WiFiGateway, WiFiSubnet, WiFiDNS1, WiFiDNS2
    //
    // If these settings are not registered (or SSID is empty), this will fall back to AP mode.
    void startWebServer()
    {
        auto getString = [this](const char *category, const char *key, const String &fallback) -> String
        {
            BaseSetting *s = findSetting(category, key);
            if (!s)
            {
                return fallback;
            }
            if (s->getType() != SettingType::STRING && s->getType() != SettingType::PASSWORD)
            {
                return fallback;
            }
            return static_cast<Config<String> *>(s)->get();
        };

        auto getBool = [this](const char *category, const char *key, bool fallback) -> bool
        {
            BaseSetting *s = findSetting(category, key);
            if (!s)
            {
                return fallback;
            }
            if (s->getType() != SettingType::BOOL)
            {
                return fallback;
            }
            return static_cast<Config<bool> *>(s)->get();
        };

        const String ssid = getString("WiFi", "WiFiSSID", "");
        const String password = getString("WiFi", "WiFiPassword", "");
        const bool useDhcp = getBool("WiFi", "WiFiUseDHCP", true);

        if (ssid.length() == 0)
        {
            CM_CORE_LOG("[W] startWebServer(): WiFi SSID is empty or not registered -> starting AP mode");
            startAccessPoint();
            return;
        }

        if (useDhcp)
        {
            startWebServer(ssid, password);
            return;
        }

        IPAddress staticIP;
        IPAddress gateway;
        IPAddress subnet;
        IPAddress dns1;
        IPAddress dns2;

        const String staticIpStr = getString("WiFi", "WiFiStaticIP", "");
        const String gatewayStr = getString("WiFi", "WiFiGateway", "");
        const String subnetStr = getString("WiFi", "WiFiSubnet", "");
        const String dns1Str = getString("WiFi", "WiFiDNS1", "");
        const String dns2Str = getString("WiFi", "WiFiDNS2", "");

        const bool okStatic = staticIP.fromString(staticIpStr);
        const bool okGw = gateway.fromString(gatewayStr);
        const bool okSubnet = subnet.fromString(subnetStr);

        if (!dns1Str.isEmpty())
        {
            if (!dns1.fromString(dns1Str))
            {
                CM_CORE_LOG("[W] startWebServer(): invalid Primary DNS: %s", dns1Str.c_str());
                dns1 = IPAddress();
            }
        }
        if (!dns2Str.isEmpty())
        {
            if (!dns2.fromString(dns2Str))
            {
                CM_CORE_LOG("[W] startWebServer(): invalid Secondary DNS: %s", dns2Str.c_str());
                dns2 = IPAddress();
            }
        }

        if (!okStatic || !okGw || !okSubnet)
        {
            CM_CORE_LOG("[W] startWebServer(): invalid static networking settings -> falling back to DHCP");
            startWebServer(ssid, password);
            return;
        }

        startWebServer(staticIP, gateway, subnet, ssid, password, dns1, dns2);
    }

    void startWebServer(const String &ssid, const String &password)
    {
        CM_CORE_LOG("[I] Starting web server with DHCP connection to %s", ssid.c_str());

        auto getInt = [this](const char *category, const char *key, int fallback) -> int
        {
            BaseSetting *s = findSetting(category, key);
            if (!s)
            {
                return fallback;
            }
            if (s->getType() != SettingType::INT)
            {
                return fallback;
            }
            return static_cast<Config<int> *>(s)->get();
        };

        const int autoRebootMinCfg = getInt("WiFi", "WiFiRb", 30);
        const unsigned long autoRebootMin = autoRebootMinCfg < 0 ? 0UL : static_cast<unsigned long>(autoRebootMinCfg);

        // Start WiFi connection non-blocking
        wifiManager.begin(10000, autoRebootMin); // 10s reconnect interval, auto-reboot timeout in minutes
        wifiManager.setCallbacks(
            [this]()
            {
                CM_CORE_LOG("Connected! Starting web server...");
                webManager.defineAllRoutes();
                otaManager.setupWebRoutes(webManager.getServer());
                // Call external connected callback if available
                extern void onWiFiConnected();
                onWiFiConnected();
            },
            [this]()
            {
                CM_CORE_LOG("Disconnected");
                // Call external disconnected callback if available
                extern void onWiFiDisconnected();
                onWiFiDisconnected();
            },
            [this]()
            {
                CM_CORE_LOG("AP Mode active");
                // Call external AP mode callback if available
                extern void onWiFiAPMode();
                onWiFiAPMode();
            });
        wifiManager.startConnection(ssid, password);

        // Initialize modules
        webManager.begin(this);
        otaManager.begin(this);
        runtimeManager.begin(this);
        WebSocketPush(true, 250);
        setPushOnConnect(true);

        CM_CORE_LOG("ConfigManager modules initialized - WiFi connecting in background");
    }

    void startWebServer(const IPAddress &staticIP, const IPAddress &gateway, const IPAddress &subnet, const String &ssid, const String &password, const IPAddress &dns1 = IPAddress(), const IPAddress &dns2 = IPAddress())
    {
        CM_CORE_LOG("Starting web server with static IP %s", staticIP.toString().c_str());

        auto getInt = [this](const char *category, const char *key, int fallback) -> int
        {
            BaseSetting *s = findSetting(category, key);
            if (!s)
            {
                return fallback;
            }
            if (s->getType() != SettingType::INT)
            {
                return fallback;
            }
            return static_cast<Config<int> *>(s)->get();
        };

        const int autoRebootMinCfg = getInt("WiFi", "WiFiRb", 30);
        const unsigned long autoRebootMin = autoRebootMinCfg < 0 ? 0UL : static_cast<unsigned long>(autoRebootMinCfg);

        wifiManager.begin(10000, autoRebootMin); // 10s reconnect interval, auto-reboot timeout in minutes
        wifiManager.setCallbacks(
            [this]()
            {
                CM_CORE_LOG("Connected! Starting web server...");
                webManager.defineAllRoutes();
                otaManager.setupWebRoutes(webManager.getServer());
                // Call external connected callback if available
                extern void onWiFiConnected();
                onWiFiConnected();
            },
            [this]()
            {
                CM_CORE_LOG("Disconnected");
                // Call external disconnected callback if available
                extern void onWiFiDisconnected();
                onWiFiDisconnected();
            },
            [this]()
            {
                CM_CORE_LOG("AP Mode active");
                // Call external AP mode callback if available
                extern void onWiFiAPMode();
                onWiFiAPMode();
            });
        wifiManager.startConnection(staticIP, gateway, subnet, ssid, password, dns1, dns2);

        webManager.begin(this);
        otaManager.begin(this);
        runtimeManager.begin(this);
        WebSocketPush(true, 250);
        setPushOnConnect(true);
    }

    void startAccessPoint(const String &apSSID = "", const String &apPassword = "")
    {
        String ssid = apSSID.isEmpty() ? (appName + "_AP") : apSSID;
        CM_CORE_LOG("[I] Starting Access Point: %s", ssid.c_str());

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
        runtimeManager.begin(this);
        WebSocketPush(true, 250);
        setPushOnConnect(true);

        webManager.defineAllRoutes();
        otaManager.begin(this);
        otaManager.setupWebRoutes(webManager.getServer());
    }

    // Non-blocking client handling
    void handleClient() { /* no-op for async */ }

    // WiFi status and control
    bool getWiFiStatus() { return wifiManager.isConnected(); }
    void reconnectWifi() { wifiManager.reconnect(); }

    // System control
    void reboot()
    {
        CM_CORE_LOG_VERBOSE("[R] Rebooting...");
        delay(1000);
        ESP.restart();
    }

    // Legacy methods for backward compatibility (simplified)
    void setCustomCss(const char *css, size_t len)
    {
        customCss = css;
        customCssLen = len;
        CM_CORE_LOG("[I] Custom CSS registered (len=%u)", (unsigned)len);
    }

    const char* getCustomCss() const { return customCss; }
    size_t getCustomCssLen() const { return customCss ? (customCssLen ? customCssLen : strlen(customCss)) : 0; }

    void clearAllFromPrefs()
    {
        for (const auto &entry : settings)
        {
            entry->setDefault(); // Reset to default values
        }
        CM_CORE_LOG("[I] All settings cleared from preferences");
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
                                bool initState = false, const String& card = String(), int order = 100,
                                const String& onLabel = String(), const String& offLabel = String()) {
        runtimeManager.defineRuntimeStateButton(group, key, label, getter, setter, initState, card, order, onLabel, offLabel);
    }

    void defineRuntimeMomentaryButton(const String& group, const String& key, const String& label,
                                      std::function<bool()> getter, std::function<void(bool)> setter,
                                      const String& card = String(), int order = 100,
                                      const String& onLabel = String(), const String& offLabel = String()) {
        runtimeManager.defineRuntimeMomentaryButton(group, key, label, getter, setter, card, order, onLabel, offLabel);
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

    void defineRuntimeIntValue(const String& group, const String& key, const String& label,
                               int minValue, int maxValue, int initValue,
                               std::function<int()> getter, std::function<void(int)> setter,
                               const String& unit = String(), const String& card = String(), int order = 100) {
        runtimeManager.defineRuntimeIntValue(group, key, label, minValue, maxValue, initValue, getter, setter, unit, card, order);
    }

    void defineRuntimeFloatValue(const String& group, const String& key, const String& label,
                                 float minValue, float maxValue, float initValue, int precision,
                                 std::function<float()> getter, std::function<void(float)> setter,
                                 const String& unit = String(), const String& card = String(), int order = 100) {
        runtimeManager.defineRuntimeFloatValue(group, key, label, minValue, maxValue, initValue, precision, getter, setter, unit, card, order);
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
        CM_CORE_LOG("[W] stopOTA not implemented in OTAManager");
    }

    // JSON export
    String toJSON(bool includeSecrets = false)
    {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();

        String catNames[20];
        int catCount = 0;
        for (const auto *s : settings)
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

            // Optional: card grouping inside the category.
            // If a setting declares a card, it will be emitted into category.cards[card].settings.
            // WebUI can render cards as separate Settings panels while keeping the persisted category token stable.
            JsonObject targetObj = catObj;
            const char* card = s->getCard();
            if (card && card[0])
            {
                JsonObject cardsObj;
                JsonVariant existingCards = catObj["cards"];
                if (existingCards.isNull())
                {
                    cardsObj = catObj.createNestedObject("cards");
                }
                else if (existingCards.is<JsonObject>())
                {
                    cardsObj = existingCards.as<JsonObject>();
                }
                else
                {
                    catObj.remove("cards");
                    cardsObj = catObj.createNestedObject("cards");
                }

                JsonObject cardObj;
                JsonVariant existingCard = cardsObj[card];
                if (existingCard.isNull())
                {
                    cardObj = cardsObj.createNestedObject(card);
                    const char* cardPretty = s->getCardPretty();
                    if (cardPretty && cardPretty[0])
                    {
                        cardObj["cardPretty"] = cardPretty;
                    }
                    cardObj["cardOrder"] = s->getCardOrder();
                }
                else if (existingCard.is<JsonObject>())
                {
                    cardObj = existingCard.as<JsonObject>();
                }
                else
                {
                    cardsObj.remove(card);
                    cardObj = cardsObj.createNestedObject(card);
                    const char* cardPretty = s->getCardPretty();
                    if (cardPretty && cardPretty[0])
                    {
                        cardObj["cardPretty"] = cardPretty;
                    }
                    cardObj["cardOrder"] = s->getCardOrder();
                }

                JsonObject settingsObj;
                JsonVariant existingCardSettings = cardObj["settings"];
                if (existingCardSettings.isNull())
                {
                    settingsObj = cardObj.createNestedObject("settings");
                }
                else if (existingCardSettings.is<JsonObject>())
                {
                    settingsObj = existingCardSettings.as<JsonObject>();
                }
                else
                {
                    cardObj.remove("settings");
                    settingsObj = cardObj.createNestedObject("settings");
                }

                targetObj = settingsObj;
            }

            if (includeSecrets || !s->isSecret())
            {
                if (s->isSecret()) {
                    CM_CORE_LOG("[DEBUG] Including secret field: %s.%s (includeSecrets=%s)",
                           s->getCategory(), s->getDisplayName(),
                           includeSecrets ? "true" : "false");
                }
                s->toJSON(targetObj);
            }
            else
            {
                CM_CORE_LOG("[DEBUG] Skipping secret field: %s.%s (includeSecrets=%s)",
                       s->getCategory(), s->getDisplayName(),
                       includeSecrets ? "true" : "false");
            }
        }

        String output;
        serializeJsonPretty(doc, output);
        return output;
    }

    String buildLiveLayoutJSON() const;

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
    void addWebSocketConnectListener(std::function<void(AsyncWebSocketClient*)> cb)
    {
        wsConnectCallbacks.push_back(std::move(cb));
    }
    void addWebSocketDisconnectListener(std::function<void(AsyncWebSocketClient*)> cb)
    {
        wsDisconnectCallbacks.push_back(std::move(cb));
    }
    bool sendWebSocketText(const String& payload)
    {
        if (!wsEnabled || !ws) {
            return false;
        }
        ws->textAll(payload);
        return true;
    }
    bool sendWebSocketText(AsyncWebSocketClient* client, const String& payload)
    {
        if (!client) {
            return false;
        }
        client->text(payload);
        return true;
    }
    size_t getWebSocketClientCount() const { return wsClients.size(); }

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
                    CM_CORE_LOG_VERBOSE("[WS] Client connect %u", client->id());
                    wsMarkSeen(client->id());
                    if (pushOnConnect) handleWebsocketPush();
                    for (auto& cb : wsConnectCallbacks) {
                        if (cb) cb(client);
                    }
                    break;
                case WS_EVT_DISCONNECT:
                    CM_CORE_LOG_VERBOSE("[WS] Client disconnect %u", client->id());
                    wsRemove(client->id());
                    for (auto& cb : wsDisconnectCallbacks) {
                        if (cb) cb(client);
                    }
                    break;
                case WS_EVT_ERROR:
                    CM_CORE_LOG_VERBOSE("[WS] Error on client %u", client->id());
                    break;
                case WS_EVT_PONG:
                    CM_CORE_LOG_VERBOSE("[WS] Pong from %u", client->id());
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
            CM_CORE_LOG_VERBOSE("[WS] Handler registered");
        }
        wsInterval = intervalMs;
        wsEnabled = true;
        CM_CORE_LOG_VERBOSE("[WS] Push enabled i=%lu ms", (unsigned long)wsInterval);
    }

    void disableWebSocketPush() { wsEnabled = false; }
    void setWebSocketInterval(uint32_t intervalMs) { wsInterval = intervalMs; }
    void setPushOnConnect(bool v) { pushOnConnect = v; }
    void setCustomLivePayloadBuilder(std::function<String()> fn) { customPayloadBuilder = fn; }
    void WebSocketPush(bool enable = true, uint32_t intervalMs = 2000)
    {
#if CM_ENABLE_WS_PUSH
        if (enable) {
            enableWebSocketPush(intervalMs);
        } else {
            disableWebSocketPush();
        }
#else
        (void)enable;
        (void)intervalMs;
#endif
    }
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
    static void setLoggerVerbose(LogCallback cb);
    static void log_message(const char *format, ...);
    static void log_verbose_message(const char *format, ...);
#else
    static void setLogger(LogCallback) {}
    static void setLoggerVerbose(LogCallback) {}
    static void log_message(const char *, ...) {}
    static void log_verbose_message(const char *, ...) {}
#endif

    void triggerLoggerTest() { CM_CORE_LOG("[I] Logger test message"); }

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
    static LogCallback loggerVerbose;
    WebHTML webhtml;
};

extern ConfigManagerClass ConfigManager;
