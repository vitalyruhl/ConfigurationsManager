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
#include <ArduinoOTA.h>
#include <Update.h>  // Required for U_FLASH
#include "html_content.h" // NOTE: WEB_HTML is now generated from webui (Vue project). Build webui and copy dist/index.html here as a string literal.
// #define development // Uncomment (or add -Ddevelopment to build flags) to enable dev-only export & runtime_meta override routes

#ifdef UNIT_TEST
// Inline fallback to ensure linker finds implementation during unit tests even if html_content.cpp filtered out
inline const char* WebHTML::getWebHTML() { return WEB_HTML; }
#endif

#define CONFIGMANAGER_VERSION "2.6.0" //2025.09.30


// ConfigOptions must be defined before any usage in Config<T>
template<typename T>
struct ConfigOptions {
    const char* keyName;
    const char* category;
    T defaultValue;
    const char* prettyName = nullptr;
    const char* prettyCat = nullptr;
    bool showInWeb = true;
    bool isPassword = false;
    void (*cb)(T) = nullptr;
    std::function<bool()> showIf = nullptr;
};
#if __cplusplus >= 201703L
// --------------------------------------------------------------------------------------------------
// OptionGroup Helper (Variant 1 Factory) - reduces boilerplate when many settings share same category
// Usage example:
//   constexpr OptionGroup WIFI{"wifi", "Network Settings"};
//   Config<String> wifiSsid( WIFI.opt<String>("ssid", "MyWiFi", "WiFi SSID") );
//   Config<String> wifiPassword( WIFI.opt<String>("password", "secret", "WiFi Password", true, true) );
//   // With dynamic visibility (showIf):
//   Config<String> staticIp( NET.opt<String>("sIP", "192.168.0.50", "Static IP", true, false, nullptr, [this](){ return !useDhcp.get(); }) );
// Notes:
//  - prettyCat automatically set from group.prettyCat
//  - prettyName optional; if nullptr display will fallback to keyName (handled in Config<T>)
//  - showInWeb defaults to true
//  - isPassword defaults to false
//  - cb (raw function pointer) & showIf (std::function) optional
//  - Works for all T supported by Config<T>
// --------------------------------------------------------------------------------------------------
struct OptionGroup {
    const char* category;
    const char* prettyCat; // may be nullptr -> falls back to category

    template<typename T>
    ConfigOptions<T> opt(const char* keyName,
                         T defaultValue,
                         const char* prettyName = nullptr,
                         bool showInWeb = true,
                         bool isPassword = false,
                         void (*cb)(T) = nullptr,
                         std::function<bool()> showIf = nullptr) const {
        return ConfigOptions<T>{
            keyName,
            category,
            defaultValue,
            prettyName,
            prettyCat ? prettyCat : category,
            showInWeb,
            isPassword,
            cb,
            showIf
        };
    }
};
#endif

#pragma once


// Server abstraction
extern AsyncWebServer server; // always async now
class ConfigManagerClass; // Forward declaration

enum class SettingType { BOOL, INT, FLOAT, STRING, PASSWORD };

template <typename T> struct TypeTraits {};
template <> struct TypeTraits<bool> { static constexpr SettingType type = SettingType::BOOL; };
template <> struct TypeTraits<int> { static constexpr SettingType type = SettingType::INT; };
template <> struct TypeTraits<float> { static constexpr SettingType type = SettingType::FLOAT; };
template <> struct TypeTraits<String> { static constexpr SettingType type = SettingType::STRING; };

// A helper function to get the length of a const char array at compile time
using LogCallback = std::function<void(const char*, ...)>;
template <size_t N>
constexpr size_t string_literal_length(const char (&)[N]) {
    return N - 1; // N includes the null terminator, so subtract 1
}

constexpr size_t const_strlen(const char* s) {
    size_t len = 0;
    while (*s) {
        s++;
        len++;
    }
    return len;
}


// ------------------------------------------------------------------------------------------------------------------
class BaseSetting {
    protected:
        bool showInWeb;
        bool isPassword;
        bool modified = false;
        const char *keyName;
        const char *category;
    const char *displayName; // 2025.08.17 - Human friendly name shown in the web interface
    const char *categoryPretty = nullptr; // 2025.09.05 - Optional pretty (display) name for the category
        SettingType type;
    bool hasKeyLengthError = false; // 2025-09-04 Bugfix: prevent buffer overflow when category + key are too long or contain unexpected whitespace
        String keyLengthErrorMsg;

        mutable std::function<void(const char*)> logger;
        void log(const char* format, ...) const {
        if (logger) {
            char buffer[256];
            va_list args;
            va_start(args, format);
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            logger(buffer);
        }
    }

    public:
    // std::function<void(const char*)> logger;

    bool hasError() const { return hasKeyLengthError; }
    const char* getError() const { return keyLengthErrorMsg.c_str(); }
    void setLogger(std::function<void(const char*)> logFunc) {
            this->logger = logFunc;
        }


    // This is constructor for backward compatibility
    BaseSetting(const char *category, const char *keyName, const char *displayName, SettingType type, bool showInWeb = true, bool isPassword = false)
        : keyName(keyName), category(category), displayName(displayName), type(type), showInWeb(showInWeb), isPassword(isPassword)
    {
        size_t catLen = strlen(category);
        size_t keyLen = strlen(keyName);
        if (catLen + keyLen + 1 > 14) {
            hasKeyLengthError = true;
            keyLengthErrorMsg = String("[ERROR] Setting will not be stored! Combined length exceeds limit.\n");
            keyLengthErrorMsg += "Category: '";
            keyLengthErrorMsg += category;
            keyLengthErrorMsg += "' (";
            keyLengthErrorMsg += String(catLen);
            keyLengthErrorMsg += " chars)\nKey: '";
            keyLengthErrorMsg += keyName;
            keyLengthErrorMsg += "' (";
            keyLengthErrorMsg += String(keyLen);
            keyLengthErrorMsg += " chars)\nTotal: ";
            keyLengthErrorMsg += String(catLen + keyLen + 1);
            keyLengthErrorMsg += " chars (max 14 allowed)\n";
            keyLengthErrorMsg += "ESP32 Preferences allows max 15 chars total (Category + _ + Key)";
        } else {
            hasKeyLengthError = false;
        }
    }

    // Overload: with categoryPretty
    BaseSetting(const char *category, const char *keyName, const char *displayName, const char *categoryPretty, SettingType type, bool showInWeb = true, bool isPassword = false)
        : keyName(keyName), category(category), displayName(displayName), categoryPretty(categoryPretty), type(type), showInWeb(showInWeb), isPassword(isPassword)
    {
        size_t catLen = strlen(category);
        size_t keyLen = strlen(keyName);
        if (catLen + keyLen + 1 > 14) {
            hasKeyLengthError = true;
            keyLengthErrorMsg = String("[ERROR] Setting will not be stored! Combined length exceeds limit.\n");
            keyLengthErrorMsg += "Category: '";
            keyLengthErrorMsg += category;
            keyLengthErrorMsg += "' (";
            keyLengthErrorMsg += String(catLen);
            keyLengthErrorMsg += " chars)\nKey: '";
            keyLengthErrorMsg += keyName;
            keyLengthErrorMsg += "' (";
            keyLengthErrorMsg += String(keyLen);
            keyLengthErrorMsg += " chars)\nTotal: ";
            keyLengthErrorMsg += String(catLen + keyLen + 1);
            keyLengthErrorMsg += " chars (max 14 allowed)\n";
            keyLengthErrorMsg += "ESP32 Preferences allows max 15 chars total (Category + _ + Key)";
        } else {
            hasKeyLengthError = false;
        }
    }

    // New template constructor for compile-time checking
    template <size_t CatLen, size_t KeyLen>
    constexpr BaseSetting(const char (&category)[CatLen], const char (&keyName)[KeyLen], const char *displayName, SettingType type, bool showInWeb = true, bool isPassword = false)
        : keyName(keyName), category(category), displayName(displayName), type(type), showInWeb(showInWeb), isPassword(isPassword) {
        static_assert(string_literal_length(category) + string_literal_length(keyName) + 1 <= 14, "Setting key and category names combined must not exceed 13 characters (plus one for the underscore).");
    }
    // Overload: with categoryPretty
    template <size_t CatLen, size_t KeyLen>
    constexpr BaseSetting(const char (&category)[CatLen], const char (&keyName)[KeyLen], const char *displayName, const char *categoryPretty, SettingType type, bool showInWeb = true, bool isPassword = false)
        : keyName(keyName), category(category), displayName(displayName), categoryPretty(categoryPretty), type(type), showInWeb(showInWeb), isPassword(isPassword) {
        static_assert(string_literal_length(category) + string_literal_length(keyName) + 1 <= 14, "Setting key and category names combined must not exceed 13 characters (plus one for the underscore).");
    }
    // Getter for categoryPretty
    const char* getCategoryPretty() const { return categoryPretty ? categoryPretty : category; }

    //---------------------------------------
    virtual ~BaseSetting() = default;
    virtual SettingType getType() const = 0;
    virtual void load(Preferences &prefs) = 0;
    virtual void save(Preferences &prefs) = 0;
    virtual void setDefault() = 0;
    virtual void toJSON(JsonObject &obj) const = 0;
    virtual bool fromJSON(const JsonVariant &value) = 0;
    // Dynamic visibility evaluation (showIf). Default: static showInWeb flag
    virtual bool isVisible() const { return showInWeb; }

    // 2025.08.17 - Accessor for display (pretty) name
    const char* getDisplayName() const {
        return displayName ? displayName : keyName;
    }

    mutable char keyBuffer[16]; // 15 chars + Null-Terminator, per-instance
    const char* getKey() const {
        // ESP32 Preferences: max 15 chars (category + '_' + key + null)
        const int MAX_TOTAL_LENGTH = 14; // 15 chars total (excluding null)

        int catLen = strlen(category);
        int keyLen = strlen(keyName);
        int maxCatLen = catLen;
        int maxKeyLen = keyLen;

        // Always truncate category to at least 1 char, then key
        if (catLen + 1 + keyLen > MAX_TOTAL_LENGTH) {
            maxCatLen = MAX_TOTAL_LENGTH - 1;
            if (maxCatLen < 1) maxCatLen = 1; // Ensure at least 1 char for category
            maxKeyLen = MAX_TOTAL_LENGTH - maxCatLen;
            if (maxKeyLen < 1) maxKeyLen = 1; // Ensure at least 1 char for key
        }

        char categoryTrimmed[13];
        char keyTrimmed[13];
        strncpy(categoryTrimmed, category, maxCatLen);
        categoryTrimmed[maxCatLen] = '\0';
        strncpy(keyTrimmed, keyName, maxKeyLen);
        keyTrimmed[maxKeyLen] = '\0';

        // Compose final key
        snprintf(keyBuffer, sizeof(keyBuffer), "%s_%s", categoryTrimmed, keyTrimmed);

        // Optionally log truncation
        if (logger) {
            if (catLen != maxCatLen || keyLen != maxKeyLen) {
                char msg[256];
                snprintf(msg, sizeof(msg), "[WARNING] getKey(): category/key truncated: '%s' (%d) + '_' + '%s' (%d) => '%s' (max 14 chars)",
                    category, catLen, keyName, keyLen, keyBuffer);
                logger(msg);
            }
        }

        return keyBuffer;
    }

    bool isSecret() const { return isPassword; }
    const char *getCategory() const { return category; }
    const char *getName() const { return keyName; }
    bool shouldShowInWeb() const { return showInWeb; }
    bool needsSave() const { return modified; }

    };

    template <typename T> class Config : public BaseSetting {
    private:
    T value;
    T defaultValue;
    void (*originalCallback)(T);
    std::function<void(T)> callback = nullptr; // 2025.08.17 - Support for std::function callbacks

    public:
    // Optional: showIf function for web visibility dependency
    std::function<bool()> showIfFunc = nullptr;
    bool shouldShowInWebDynamic() const {
        if (showIfFunc) return showIfFunc();
        return showInWeb;
    }

    T get() const { return value; }

    void setCallback(std::function<void(T)> cb) {
        callback = cb;
    }

    void set(T newVal) {
        if (value != newVal) {
            value = newVal;
            modified = true;
            if (callback) callback(newVal);
            if (originalCallback) originalCallback(newVal);
        }
    }

    // NOTE: ConfigOptions field order matters for designated initialization in C++17 (GNU extension).
    // Keep declaration order consistent: keyName, category, defaultValue, prettyName, prettyCat, showInWeb, isPassword, cb, showIf.
    // When adding new fields, update all initializers accordingly.

    Config(const ConfigOptions<T>& opts)
        : BaseSetting(
            opts.category,
            opts.keyName,
            opts.prettyName ? opts.prettyName : opts.keyName,
            opts.prettyCat ? opts.prettyCat : opts.category,
            TypeTraits<T>::type,
            opts.showInWeb,
            opts.isPassword),
          value(opts.defaultValue),
          defaultValue(opts.defaultValue),
          originalCallback(opts.cb),
          showIfFunc(opts.showIf)
    {
        if (hasError() && logger) {
            logger(getError());
        }
    }

    SettingType getType() const override { return TypeTraits<T>::type; }

    void load(Preferences &prefs) override {
        String key = getKey();
        bool exists = prefs.isKey(key.c_str());
        if (!exists) {
            // Key not yet stored – keep defaultValue (already set in ctor)
            value = defaultValue;
            modified = false;
            return;
        }
        switch (TypeTraits<T>::type) {
            case SettingType::BOOL:
                if constexpr (std::is_same_v<T, bool>) value = prefs.getBool(key.c_str(), defaultValue);
                break;
            case SettingType::INT:
                if constexpr (std::is_same_v<T, int>) value = prefs.getInt(key.c_str(), defaultValue);
                break;
            case SettingType::FLOAT:
                if constexpr (std::is_same_v<T, float>) {
                    // Preferences has no float, store as string
                    String stored = prefs.getString(key.c_str(), String(defaultValue, 6));
                    value = stored.toFloat();
                }
                break;
            case SettingType::STRING:
            case SettingType::PASSWORD:
                if constexpr (std::is_same_v<T, String>) value = prefs.getString(key.c_str(), defaultValue);
                break;
        }
        modified = false;
    }

    void save(Preferences &prefs) override {
        String key = getKey();
        switch (TypeTraits<T>::type) {
            case SettingType::BOOL:
                if constexpr (std::is_same_v<T, bool>) prefs.putBool(key.c_str(), value);
                break;
            case SettingType::INT:
                if constexpr (std::is_same_v<T, int>) prefs.putInt(key.c_str(), value);
                break;
            case SettingType::FLOAT:
                if constexpr (std::is_same_v<T, float>) {
                    char buf[32];
                    dtostrf(value, 0, 6, buf);
                    prefs.putString(key.c_str(), buf);
                }
                break;
            case SettingType::STRING:
            case SettingType::PASSWORD:
                if constexpr (std::is_same_v<T, String>) prefs.putString(key.c_str(), value);
                break;
        }
        modified = false;
    }

    void setDefault() override {
        value = defaultValue;
        modified = true;
    }

    // 2025.08.17 - Updated toJSON with display keyName
    void toJSON(JsonObject &obj) const override {
        JsonObject settingObj = obj.createNestedObject(keyName);
        if (isSecret()) {
            settingObj["value"] = "***";
        } else {
            settingObj["value"] = value;
        }
        settingObj["displayName"] = getDisplayName();
        settingObj["isPassword"] = isSecret();
    }

    bool fromJSON(const JsonVariant &val) override {
        if (val.isNull()) return false;

        // Prevent masked "***" from being stored as real password
        if (isSecret() && val.is<const char*>() && strcmp(val.as<const char*>(), "***") == 0) {
            return false;// Masked password - no change
        }

        if constexpr (std::is_same_v<T, String>) {
            set(val.as<String>());
        } else {
            set(val.as<T>());
        }
        return true;
    }
    bool isVisible() const override {
        if (showIfFunc) return showIfFunc();
        return showInWeb;
    }
};

// --------------------------------------------------------------------------------------------------
// Visibility helper factories (global scope) - placed after Config<T> so that Config is complete.
// Usage in user code (main.cpp):
//   staticIp( WIFI_GROUP.opt<String>("sIP", "192.168.0.50", "Static IP", true, false, nullptr, showIfFalse(useDhcp)) );
//   advanced( GROUP.opt<int>("adv", 1, "Advanced", true, false, nullptr, showIfTrue(expertFlag)) );
// These helpers avoid repetitive lambdas like: [this](){ return !flag.get(); }
// --------------------------------------------------------------------------------------------------
inline std::function<bool()> showIfTrue (const Config<bool>& flag){ return [&flag](){ return flag.get(); }; }
inline std::function<bool()> showIfFalse(const Config<bool>& flag){ return [&flag](){ return !flag.get(); }; }
// --------------------------------------------------------------------------------------------------

class ConfigManagerClass {

public:

    ConfigManagerClass(){
    #ifdef APP_NAME
        _appName = APP_NAME;
    #else
        _appName = "ConfigManager";
    #endif
    }

    BaseSetting *findSetting(const String &category, const String &key) {
        for (auto *s : settings)
        {
            if (String(s->getCategory()) == category && String(s->getName()) == key)
            {
                return s;
            }
        }
        return nullptr;
    }

    //new logging-callback functions
    typedef std::function<void(const char*)> LogCallback;

    static void setLogger(LogCallback cb) {
        logger = cb;
    }

    static void log_message(const char* format, ...) {
        if (!logger) return;

        char buffer[255];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        logger(buffer);
    }

    void setAppName(const String& name){
        if(name.length()) _appName = name;
        else _appName = "ConfigManager";
    }
    const String& getAppName() const { return _appName; }

    void triggerLoggerTest() {
       log_message("Test message abcdefghijklmnop");
    }

    // 2025.09.04 - error handling for settings with key length issues
    void addSetting(BaseSetting *setting) {
        // Always truncate category/key in getKey(), so we check for uniqueness after truncation
        const char* truncatedKey = setting->getKey();
        // Check for uniqueness among already added settings
        for (auto* s : settings) {
            if (strcmp(s->getKey(), truncatedKey) == 0) {
                log_message("[ERROR] Setting with key '%s' already exists after truncation. Not adding duplicate.", truncatedKey);
                log_message("[ERROR] This setting will not be stored or available in the web interface.");
                return;
            }
        }
        // If unique, add
        settings.push_back(setting);
        setting->setLogger([this](const char* msg) {
            this->log_message("%s", msg);
        });
    }

    void loadAll() {
    // 2025-09-04 Bugfix note: Previously repeated NOT_FOUND messages (Preferences getString len fail) flooded the log.
    // We now check for key existence first to avoid noisy output.
        prefs.begin("config", false); // Read-write mode
        for (auto *s : settings) {
            log_message("loadAll(): Loading %s (type: %d)", s->getKey(), static_cast<int>(s->getType()));
            s->load(prefs);

            if (!prefs.isKey(s->getKey())) {
                s->save(prefs); // Save default value if key not found
                log_message("loadAll(): Saved default value for: %s", s->getKey());
            }

        }
        prefs.end();
    }

    void saveAll() {
        prefs.begin("config", false);
        for (auto *s : settings) {
            if (!s->needsSave()) continue;
            if (s->isSecret() && s->getType() == SettingType::STRING) {
                Config<String>* strSetting = static_cast<Config<String>*>(s);
                String val = strSetting->get();
                if (val.isEmpty() || val == "***") {// 05.09.2025 skip saving this password if its empty or masked
                    continue;
                }
            }
            s->save(prefs);
        }
        prefs.end();
    }

    bool getWiFiStatus() {
        return WiFi.status() == WL_CONNECTED;
    }

    void reboot() {
        log_message("🔄 Rebooting...");
        delay(1000);
        ESP.restart();
    }

    void handleClient() { /* no-op for async */ }

    // WebSocket push (always available)
    void handleWebsocketPush(){
        if(!_wsEnabled) return;
        unsigned long now = millis();
        if (now - _wsLastPush < _wsInterval) return;
        _wsLastPush = now;
        pushRuntimeNow();
    }
    void enableWebSocketPush(uint32_t intervalMs = 2000){
        if(!_wsInitialized){
            _ws = new AsyncWebSocket("/ws");
            _ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len){
                if(type == WS_EVT_CONNECT){ log_message("[WS] Client connect %u", client->id()); if(_pushOnConnect) pushRuntimeNow(); }
                else if(type == WS_EVT_DISCONNECT){ log_message("[WS] Client disconnect %u", client->id()); }
                else if(type == WS_EVT_DATA){ /* ignore */ }
            });
            server.addHandler(_ws);
            _wsInitialized = true;
            log_message("[WS] Handler registered at /ws");
        }
        _wsInterval = intervalMs;
        _wsEnabled = true;
        log_message("[WS] Push enabled interval=%lu ms", (unsigned long)_wsInterval);
        // Immediate first push so UI does not wait for first interval
        pushRuntimeNow();
    }
    void disableWebSocketPush(){ _wsEnabled = false; }
    void setWebSocketInterval(uint32_t intervalMs){ _wsInterval = intervalMs; }
    void setPushOnConnect(bool v){ _pushOnConnect = v; }
    // Optional external payload hook
    void setCustomLivePayloadBuilder(std::function<String()> fn){ _customPayloadBuilder = fn; }
    void pushRuntimeNow(){
        if(!_wsInitialized || !_wsEnabled) return;
        String payload;
        if(_customPayloadBuilder){ payload = _customPayloadBuilder(); }
        else {
            payload = runtimeValuesToJSON();
        }
        _ws->textAll(payload);
    }

    void handleClientSecure() { /* future TLS hook */ }

    void reconnectWifi() {
        log_message("🔄 Reconnecting to WiFi...");
        WiFi.disconnect();
        delay(1000);
        WiFi.reconnect();
    }

    String toJSON(bool includeSecrets = false) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        // Track which categories we've already set pretty names for (Arduino compatible)
        String catNames[20]; // up to 20 categories
        int catCount = 0;
        for (auto *s : settings) {
            bool visible = s->isVisible();
            const char *category = s->getCategory();
            const char *keyName = s->getName();
            if (!root[category].is<JsonObject>()) root.createNestedObject(category);
            JsonObject cat = root[category];
            // Set categoryPretty only once per category
            bool found = false;
            for (int i = 0; i < catCount; ++i) {
                if (catNames[i] == category) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                const char* pretty = s->getCategoryPretty();
                if (pretty && strcmp(pretty, category) != 0) {
                    cat["categoryPretty"] = pretty;
                }
                if (catCount < 20) catNames[catCount++] = category;
            }
            s->toJSON(cat);
            if (cat[keyName].is<JsonObject>()) {
                cat[keyName]["showIf"] = visible;
            }
        }
        String output;
        serializeJsonPretty(doc, output);
        return output;
    }

    bool fromJSON(JsonDocument &doc) {
        bool changed = false;
        for (auto *s : settings) {
            JsonVariant val = doc[s->getCategory()][s->getName()];
            changed |= s->fromJSON(val);
        }
        return changed;
    }

    void startAccessPoint() {
        startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", "config1234");
    }

    void startAccessPoint(const String &pwd) {
        startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", pwd);
    }

    void startAccessPoint(const String &APName, const String &pwd) {
        startAccessPoint("192.168.4.1", "255.255.255.0", APName, pwd);
    }

    void startAccessPoint(const String &ipStr, const String &mask, const String &APName, const String &pwd) {
        log_message("🌐 Configuring AP %s...\n", APName.c_str());
        log_message("🌐 Setting static IP %s...\n", ipStr.c_str());
        log_message("🌐 Setting subnet mask %s...\n", mask.c_str());
        log_message("🌐 Setting password %s...\n", pwd.c_str());
        log_message("🌐 Setting gateway %s...\n", ipStr.c_str());

        IPAddress localIP, gateway, subnet;
        localIP.fromString(ipStr);
        gateway = localIP;
        subnet.fromString(mask);

        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(localIP, gateway, subnet);

        if (strcmp(pwd.c_str(), "") == 0) {
            log_message("🌐 AP without password");
            WiFi.softAP(APName);
        } else {
            log_message("🌐 AP with password: %s", pwd.c_str());
            WiFi.softAP(APName, pwd);
        }
        log_message("🌐 AP started at: %s", WiFi.softAPIP().toString().c_str());
        defineRoutes();
        server.begin();
    }

    // Legacy simplified static config (IP + mask only). Gateway assumed as first host in subnet and DNS forced to 8.8.8.8.
    void startWebServer(const String &ipStr, const String &mask, const String &ssid, const String &password) {
        IPAddress ip, gateway, subnet, dns(8,8,8,8);
        ip.fromString(ipStr);
        subnet.fromString(mask);
        // Derive gateway: keep network part of ip and .1 as common default
        gateway = IPAddress((uint32_t)ip & (uint32_t)subnet | 0x01000000); // Build x.y.z.1
        log_message("🌐 Static (simplified) IP: %s Gateway(auto): %s Mask: %s", ip.toString().c_str(), gateway.toString().c_str(), subnet.toString().c_str());
        WiFi.config(ip, gateway, subnet, dns);
        connectAndStart(ssid, password);
    }

    // Explicit static config without custom DNS (defaults to 8.8.8.8)
    void startWebServer(const String &ipStr, const String &gatewayStr, const String &mask, const String &ssid, const String &password) {
        IPAddress ip, gateway, subnet, dns(8,8,8,8);
        ip.fromString(ipStr);
        gateway.fromString(gatewayStr);
        subnet.fromString(mask);
        log_message("🌐 Static IP: %s Gateway: %s Mask: %s", ip.toString().c_str(), gateway.toString().c_str(), subnet.toString().c_str());
        WiFi.config(ip, gateway, subnet, dns);
        connectAndStart(ssid, password);
    }

    // Explicit full static configuration including DNS server
    void startWebServer(const String &ipStr, const String &gatewayStr, const String &mask, const String &dnsServer, const String &ssid, const String &password) {
        IPAddress ip, gateway, subnet, dns;
        ip.fromString(ipStr);
        gateway.fromString(gatewayStr);
        subnet.fromString(mask);
        dns.fromString(dnsServer);
        log_message("🌐 Static IP: %s Gateway: %s Mask: %s DNS: %s", ip.toString().c_str(), gateway.toString().c_str(), subnet.toString().c_str(), dns.toString().c_str());
        WiFi.config(ip, gateway, subnet, dns);
        connectAndStart(ssid, password);
    }

    void startWebServer(const String &ssid, const String &password) {
        log_message("🌐 DHCP mode active - Connecting to WiFi...");
        WiFi.mode(WIFI_STA); // Set WiFi mode to Station bugfix for unit test
        WiFi.begin(ssid.c_str(), password.c_str());

        unsigned long startAttemptTime = millis();
        const unsigned long timeout = 10000;

        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
            delay(250);
            log_message(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            log_message("\n✅ WiFi connected via DHCP!");
            log_message("🌐 IP address: %s", WiFi.localIP().toString().c_str());

            defineRoutes();
            server.begin();
        } else {
            log_message("\n❌ DHCP connection failed!");
        }
    }

private:
    void connectAndStart(const String &ssid, const String &password) {
        log_message("🔌 Connecting to WiFi SSID: %s", ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());
        unsigned long startAttemptTime = millis();
        const unsigned long timeout = 10000;
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
            delay(250);
            log_message(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
            log_message("\n✅ WiFi connected!");
            log_message("🌐 IP address: %s", WiFi.localIP().toString().c_str());
            log_message("🌐 Defining routes...");
            defineRoutes();
            log_message("🌐 Starting web server...");
            server.begin();
            log_message("🖥️  Web server running at: %s", WiFi.localIP().toString().c_str());
        } else {
            log_message("\n❌ Failed to connect to WiFi!");
        }
    }
public:

    void defineRoutes() {
        log_message("🌐 Defining routes...");
        // Serve version at /version (always async)
        server.on("/version", HTTP_GET, [this](AsyncWebServerRequest *request){
            String payload = _appName;
            if(payload.length()) payload += " ";
            payload += CONFIGMANAGER_VERSION;
            request->send(200, "text/plain", payload);
        });
        struct OtaUploadContext {
            bool authorized = false;
            bool began = false;
            bool success = false;
            bool hasError = false;
            String errorReason;
            size_t written = 0;
            int statusCode = 200;
        };

        server.on("/ota_update", HTTP_GET, [this](AsyncWebServerRequest *request){
            if(!_otaEnabled){
                request->send(403, "application/json", "{\"status\":\"error\",\"reason\":\"ota_disabled\"}");
                return;
            }
            request->send(200, "application/json", "{\"status\":\"ready\"}");
        });

        server.on("/ota_update", HTTP_POST,
            [this](AsyncWebServerRequest *request){
                auto *ctx = static_cast<OtaUploadContext*>(request->_tempObject);
                auto cleanup = [request, ctx](){
                    if(ctx){ delete ctx; request->_tempObject = nullptr; }
                };

                if(!_otaEnabled){
                    request->send(403, "application/json", "{\"status\":\"error\",\"reason\":\"ota_disabled\"}");
                    cleanup();
                    return;
                }

                if(!ctx){
                    request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"no_context\"}");
                    cleanup();
                    return;
                }

                if(ctx->hasError || Update.hasError()){
                    int code = ctx->statusCode ? ctx->statusCode : 500;
                    if(ctx->began && !ctx->hasError){ Update.abort(); }
                    String reason = ctx->errorReason.length() ? ctx->errorReason : String("upload_failed");
                    log_message("❌ OTA HTTP error (%d): %s", code, reason.c_str());
                    String payload = String("{\"status\":\"error\",\"reason\":\"") + reason + "\"}";
                    request->send(code, "application/json", payload);
                    cleanup();
                    return;
                }

                if(!ctx->began || !ctx->success){
                    log_message("❌ OTA HTTP upload incomplete");
                    request->send(500, "application/json", "{\"status\":\"error\",\"reason\":\"incomplete\"}");
                    cleanup();
                    return;
                }

                AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\",\"action\":\"reboot\"}");
                response->addHeader("Connection", "close");
                request->onDisconnect([this](){
                    log_message("🔁 OTA HTTP: client disconnected, rebooting...");
                    delay(500);
                    reboot();
                });
                request->send(response);
                log_message("✅ OTA HTTP upload success (%lu bytes)", static_cast<unsigned long>(ctx->written));
                cleanup();
            },
            [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
                OtaUploadContext *ctx = static_cast<OtaUploadContext*>(request->_tempObject);

                if(index == 0){
                    ctx = new OtaUploadContext();
                    request->_tempObject = ctx;

                    if(!_otaEnabled){
                        ctx->hasError = true;
                        ctx->statusCode = 403;
                        ctx->errorReason = "ota_disabled";
                        return;
                    }

                    if(!_otaPassword.isEmpty()){
                        AsyncWebHeader *hdr = request->getHeader("X-OTA-PASSWORD");
                        if(!hdr){
                            ctx->hasError = true;
                            ctx->statusCode = 401;
                            ctx->errorReason = "missing_password";
                            return;
                        }
                        if(hdr->value() != _otaPassword){
                            ctx->hasError = true;
                            ctx->statusCode = 401;
                            ctx->errorReason = "unauthorized";
                            return;
                        }
                    }
                    ctx->authorized = true;

                    size_t expected = request->contentLength();
                    if(expected == 0){
                        ctx->hasError = true;
                        ctx->statusCode = 400;
                        ctx->errorReason = "empty_upload";
                        return;
                    }

                    if(!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)){
                        ctx->hasError = true;
                        ctx->statusCode = 500;
                        ctx->errorReason = "begin_failed";
                        Update.printError(Serial);
                        return;
                    }

                    ctx->began = true;
                    log_message("📦 OTA HTTP upload start: %s (%lu bytes)", filename.c_str(), static_cast<unsigned long>(expected));
                }

                if(!ctx || ctx->hasError || !ctx->authorized){
                    return;
                }

                if(len){
                    if(Update.write(data, len) != len){
                        ctx->hasError = true;
                        ctx->statusCode = 500;
                        ctx->errorReason = "write_failed";
                        Update.printError(Serial);
                        return;
                    }
                    ctx->written += len;
                }

                if(final){
                    if(Update.end(true)){
                        ctx->success = true;
                    } else {
                        ctx->hasError = true;
                        ctx->statusCode = 500;
                        ctx->errorReason = "end_failed";
                        Update.printError(Serial);
                    }
                }
            }
        );

        // Reset to Defaults
        server.on("/config/reset", HTTP_POST, [this](AsyncWebServerRequest *request){ for (auto *s : settings) s->setDefault(); saveAll(); log_message("🌐 All settings reset to default"); request->send(200, "application/json", "{\"status\":\"reset\"}"); });

        // Apply single setting (async body handler)
        server.on("/config/apply", HTTP_POST,
            [this](AsyncWebServerRequest *request){ /* response sent in body handler */ },
            NULL,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                if(index == 0){ request->_tempObject = new String(); static_cast<String*>(request->_tempObject)->reserve(total); }
                String *body = static_cast<String*>(request->_tempObject);
                body->concat(String((const char*)data).substring(0,len));
                if(index + len == total){
                    AsyncWebParameter* pCat = request->getParam("category");
                    if(!pCat) pCat = request->getParam("category", true);
                    AsyncWebParameter* pKey = request->getParam("key");
                    if(!pKey) pKey = request->getParam("key", true);
                    if(!pCat || !pKey){ request->send(400, "application/json", "{\"status\":\"missing_params\"}"); delete body; request->_tempObject=nullptr; return; }
                    String category = pCat->value();
                    String key = pKey->value();
                    log_message("🌐 Apply: %s/%s", category.c_str(), key.c_str());
                    DynamicJsonDocument doc(256);
                    DeserializationError err = deserializeJson(doc, *body);
                    auto isIntegerString = [](const String &s)->bool { if(!s.length()) return false; int st=0; if(s[0]=='-'&&s.length()>1) st=1; for(int i=st;i<(int)s.length();++i){ if(s[i]<'0'||s[i]>'9') return false;} return true; };
                    JsonVariant val;
                    if(!err){ if(doc.containsKey("value")) val = doc["value"]; else val = doc.as<JsonVariant>(); }
                    else { doc.clear(); if(body->equalsIgnoreCase("true")||body->equalsIgnoreCase("false")) doc["value"] = body->equalsIgnoreCase("true"); else if(isIntegerString(*body)) doc["value"] = body->toInt(); else doc["value"] = *body; val = doc["value"]; }
                    for(auto *s: settings){ if(String(s->getCategory())==category && String(s->getName())==key){ if(s->fromJSON(val)){ String resp = String("{\"status\":\"ok\",\"action\":\"apply\",\"category\":\"")+category+"\",\"key\":\""+key+"\"}"; request->send(200, "application/json", resp); delete body; request->_tempObject=nullptr; return;} else { request->send(400, "application/json", "{\"status\":\"error\",\"action\":\"apply\",\"reason\":\"invalid_value\"}"); delete body; request->_tempObject=nullptr; return;} } }
                    request->send(404, "application/json", "{\"status\":\"error\",\"action\":\"apply\",\"reason\":\"not_found\"}"); delete body; request->_tempObject=nullptr;
                }
            }
        );

        // Apply all (async)
        server.on("/config/apply_all", HTTP_POST,
            [this](AsyncWebServerRequest *request){}, NULL,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                if(index==0){ request->_tempObject = new String(); static_cast<String*>(request->_tempObject)->reserve(total); }
                String *body = static_cast<String*>(request->_tempObject);
                body->concat(String((const char*)data).substring(0,len));
                if(index+len==total){
                    DynamicJsonDocument doc(1024);
                    deserializeJson(doc, *body);
                    if(fromJSON(doc)) request->send(200, "application/json", "{\"status\":\"ok\",\"action\":\"apply_all\"}"); else request->send(400, "application/json", "{\"status\":\"error\",\"action\":\"apply_all\",\"reason\":\"invalid\"}");
                    delete body; request->_tempObject=nullptr;
                }
            }
        );

    // Save single setting (async)
        server.on("/config/save", HTTP_POST,
            [this](AsyncWebServerRequest *request){}, NULL,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                if(index==0){ request->_tempObject = new String(); static_cast<String*>(request->_tempObject)->reserve(total);} 
                String *body = static_cast<String*>(request->_tempObject);
                body->concat(String((const char*)data).substring(0,len));
                if(index+len==total){
                    AsyncWebParameter* pCat = request->getParam("category");
                    if(!pCat) pCat = request->getParam("category", true);
                    AsyncWebParameter* pKey = request->getParam("key");
                    if(!pKey) pKey = request->getParam("key", true);
                    if(!pCat || !pKey){ request->send(400, "application/json", "{\"status\":\"missing_params\"}"); delete body; request->_tempObject=nullptr; return; }
                    String category = pCat->value();
                    String key = pKey->value();
                    DynamicJsonDocument doc(256);
                    DeserializationError err = deserializeJson(doc, *body);
                    auto isIntegerString = [](const String &s)->bool { if(!s.length()) return false; int st=0; if(s[0]=='-'&&s.length()>1) st=1; for(int i=st;i<(int)s.length();++i){ if(s[i]<'0'||s[i]>'9') return false;} return true; };
                    JsonVariant val; if(!err){ if(doc.containsKey("value")) val = doc["value"]; else val = doc.as<JsonVariant>(); } else { doc.clear(); if(body->equalsIgnoreCase("true")||body->equalsIgnoreCase("false")) doc["value"] = body->equalsIgnoreCase("true"); else if(isIntegerString(*body)) doc["value"] = body->toInt(); else doc["value"] = *body; val = doc["value"]; }
                    for(auto *s: settings){ if(String(s->getCategory())==category && String(s->getName())==key){ if(s->fromJSON(val)){ prefs.begin("config", false); s->save(prefs); prefs.end(); String resp = String("{\"status\":\"ok\",\"action\":\"save\",\"category\":\"")+category+"\",\"key\":\""+key+"\"}"; request->send(200, "application/json", resp); delete body; request->_tempObject=nullptr; return;} else { request->send(400, "application/json", "{\"status\":\"error\",\"action\":\"save\",\"reason\":\"invalid_value\"}"); delete body; request->_tempObject=nullptr; return;} } }
                    request->send(404, "application/json", "{\"status\":\"error\",\"action\":\"save\",\"reason\":\"not_found\"}"); delete body; request->_tempObject=nullptr;
                }
        }
    );

    // Reboot
    server.on("/config/reboot", HTTP_POST, [this](AsyncWebServerRequest *request){
        request->send(200, "application/json", "{\"status\":\"rebooting\"}");
        log_message("🔄 Device rebooting...");
        // Schedule reboot after short delay (simple blocking delay acceptable here)
        delay(100);
        reboot();
    });

        // Configuration & runtime JSON endpoints
        server.on("/config.json", HTTP_GET, [this](AsyncWebServerRequest *request){ request->send(200, "application/json", toJSON()); });
        server.on("/runtime.json", HTTP_GET, [this](AsyncWebServerRequest *request){ request->send(200, "application/json", runtimeValuesToJSON()); });
        server.on("/runtime_meta.json", HTTP_GET, [this](AsyncWebServerRequest *request){ request->send(200, "application/json", runtimeMetaToJSON()); });
        // Interactive runtime control endpoints
        server.on("/runtime_action/button", HTTP_POST, [this](AsyncWebServerRequest *request){
            AsyncWebParameter *pg = request->getParam("group"); if(!pg) pg = request->getParam("group", true);
            AsyncWebParameter *pk = request->getParam("key"); if(!pk) pk = request->getParam("key", true);
            if(!pg || !pk){ request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_params\"}"); return; }
            String g = pg->value(); String k = pk->value();
            for(auto &b : _runtimeButtons){ if(b.group==g && b.key==k){ if(b.onPress) b.onPress(); request->send(200, "application/json", "{\"status\":\"ok\"}"); return; } }
            request->send(404, "application/json", "{\"status\":\"error\",\"reason\":\"not_found\"}");
        });
        server.on("/runtime_action/checkbox", HTTP_POST, [this](AsyncWebServerRequest *request){
            AsyncWebParameter *pg = request->getParam("group"); if(!pg) pg = request->getParam("group", true);
            AsyncWebParameter *pk = request->getParam("key"); if(!pk) pk = request->getParam("key", true);
            AsyncWebParameter *pv = request->getParam("value"); if(!pv) pv = request->getParam("value", true);
            if(!pg || !pk || !pv){ request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_params\"}"); return; }
            bool v = pv->value().equalsIgnoreCase("true") || pv->value()=="1";
            String g = pg->value(); String k = pk->value();
            for(auto &c : _runtimeCheckboxes){ if(c.group==g && c.key==k){ if(c.setter) c.setter(v); request->send(200, "application/json", "{\"status\":\"ok\"}"); return; } }
            request->send(404, "application/json", "{\"status\":\"error\",\"reason\":\"not_found\"}");
        });
        // Stateful button toggle
        server.on("/runtime_action/state_button", HTTP_POST, [this](AsyncWebServerRequest *request){
            AsyncWebParameter *pg = request->getParam("group"); if(!pg) pg = request->getParam("group", true);
            AsyncWebParameter *pk = request->getParam("key"); if(!pk) pk = request->getParam("key", true);
            AsyncWebParameter *pv = request->getParam("value"); if(!pv) pv = request->getParam("value", true);
            if(!pg || !pk || !pv){ request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_params\"}"); return; }
            bool v = pv->value().equalsIgnoreCase("true") || pv->value()=="1";
            String g = pg->value(); String k = pk->value();
            for(auto &sb : _runtimeStateButtons){ if(sb.group==g && sb.key==k){ if(sb.setter) sb.setter(v); request->send(200, "application/json", "{\"status\":\"ok\"}"); return; } }
            request->send(404, "application/json", "{\"status\":\"error\",\"reason\":\"not_found\"}");
        });
        // Int slider update
        server.on("/runtime_action/int_slider", HTTP_POST, [this](AsyncWebServerRequest *request){
            AsyncWebParameter *pg = request->getParam("group"); if(!pg) pg = request->getParam("group", true);
            AsyncWebParameter *pk = request->getParam("key"); if(!pk) pk = request->getParam("key", true);
            AsyncWebParameter *pv = request->getParam("value"); if(!pv) pv = request->getParam("value", true);
            if(!pg || !pk || !pv){ request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_params\"}"); return; }
            String g = pg->value(); String k = pk->value(); int val = pv->value().toInt();
            for(auto &is : _runtimeIntSliders){ if(is.group==g && is.key==k){ if(is.setter) is.setter(val); request->send(200, "application/json", "{\"status\":\"ok\"}"); return; } }
            request->send(404, "application/json", "{\"status\":\"error\",\"reason\":\"not_found\"}");
        });
        // Float slider update
        server.on("/runtime_action/float_slider", HTTP_POST, [this](AsyncWebServerRequest *request){
            AsyncWebParameter *pg = request->getParam("group"); if(!pg) pg = request->getParam("group", true);
            AsyncWebParameter *pk = request->getParam("key"); if(!pk) pk = request->getParam("key", true);
            AsyncWebParameter *pv = request->getParam("value"); if(!pv) pv = request->getParam("value", true);
            if(!pg || !pk || !pv){ request->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"missing_params\"}"); return; }
            String g = pg->value(); String k = pk->value(); float val = pv->value().toFloat();
            for(auto &fs : _runtimeFloatSliders){ if(fs.group==g && fs.key==k){ if(fs.setter) fs.setter(val); request->send(200, "application/json", "{\"status\":\"ok\"}"); return; } }
            request->send(404, "application/json", "{\"status\":\"error\",\"reason\":\"not_found\"}");
        });

        server.on("/user_theme.css", HTTP_GET, [this](AsyncWebServerRequest *request){
            if(getCustomCss() && getCustomCssLen()>0){
                log_message("[THEME] Serving user_theme.css (%u bytes)", (unsigned)getCustomCssLen());
                request->send(200, "text/css", String(getCustomCss()));
            } else {
                log_message("[THEME] No custom CSS configured (204)");
                request->send(204); // No Content
            }
        });

        // Save all settings
        server.on("/config/save_all", HTTP_POST,
            [this](AsyncWebServerRequest *request){}, NULL,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                if(index==0){ request->_tempObject = new String(); static_cast<String*>(request->_tempObject)->reserve(total);} 
                String *body = static_cast<String*>(request->_tempObject);
                body->concat(String((const char*)data).substring(0,len));
                if(index+len==total){
                    DynamicJsonDocument doc(1024);
                    deserializeJson(doc, *body);
                    if(fromJSON(doc)){ saveAll(); request->send(200, "application/json", "{\"status\":\"ok\",\"action\":\"save_all\"}"); } else { request->send(400, "application/json", "{\"status\":\"error\",\"action\":\"save_all\",\"reason\":\"invalid\"}"); }
                    delete body; request->_tempObject=nullptr;
                }
            }
        );

        // Root (serves embedded SPA)
        server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){ request->send_P(200, "text/html", webhtml.getWebHTML()); });

        server.on("/", HTTP_POST,
            [this](AsyncWebServerRequest *request){ /* response sent after body parsed */ },
            NULL,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                if(index==0){ request->_tempObject = new String(); static_cast<String*>(request->_tempObject)->reserve(total);} 
                String *body = static_cast<String*>(request->_tempObject);
                body->concat(String((const char*)data).substring(0,len));
                if(index+len==total){
                    log_message("🌐 Received POST request to root route");
                    log_message("🌐 Body: %s", body->c_str());
                    request->send(200, "text/plain", "OK");
                    delete body; request->_tempObject=nullptr;
                }
            }
        );

        log_message("🌐 Routes defined successfully");
    }

    void remove(String category, String key) {
        for (auto it = settings.begin(); it != settings.end(); ++it) {
            if (String((*it)->getCategory()) == category && String((*it)->getName()) == key) {
                settings.erase(it);
                break;
            }
        }
    }

    void remove(BaseSetting *s) {
        for (auto it = settings.begin(); it != settings.end(); ++it) {
            if (*it == s) {
                settings.erase(it);
                break;
            }
        }
    }

    void clearAll() {
        for (auto *s : settings) {
            delete s;
        }
        settings.clear();
        log_message("🌐 All settings cleared");
        prefs.begin("config", false);
        prefs.clear();
        prefs.end();
        log_message("🌐 Preferences cleared");
    }

    void clearAllFromPrefs() {
        prefs.begin("config", false);
        prefs.clear();
        prefs.end();
        log_message("🌐 All preferences cleared");
    }

    bool isOTAInitialized() const { return _otaInitialized; }

    String getOTAStatus() const {
        if (!_otaEnabled) return "disabled";
        if (!_otaInitialized) return "not initialized";
        return "active on port 3232";
    }

    void setupOTA(const String& hostname = "", const String& password = "") {
        _otaHostname = hostname.isEmpty() ? "esp32-device" : hostname;
        _otaPassword = password;
        _otaEnabled = true;
        log_message("🛜 OTA Enabled (Hostname: %s)", _otaHostname.c_str());
    }

    void handleOTA() {
        // if (!_otaEnabled || _otaInitialized) return;

        if (!_otaEnabled) {
            log_message("OTA: Not enabled");
            return;
        }

        // Initialize OTA only when WiFi is connected
        if (WiFi.status() == WL_CONNECTED) {

            ArduinoOTA
                .onStart([this]() {
                    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
                    log_message("📡 OTA Update Start: %s", type.c_str());
                })
                .onEnd([this]() {
                    log_message("OTA Update finished");
                })
                .onProgress([this](unsigned int progress, unsigned int total) {
                    log_message("OTA Progress: %u%%", (progress * 100) / total);
                })

                .onError([this](ota_error_t error) {
                    log_message("OTA Error[%u]:", error);
                    if (error == OTA_AUTH_ERROR) log_message("Authentication failed");
                    else if (error == OTA_BEGIN_ERROR) log_message("Begin failed");
                    else if (error == OTA_CONNECT_ERROR) log_message("Connection failed");
                    else if (error == OTA_RECEIVE_ERROR) log_message("Receive failed");
                    else if (error == OTA_END_ERROR) log_message("End failed");
                });

            if (_otaHostname.isEmpty()) {
                ArduinoOTA.setHostname("esp32-dev");
            }
            else{
                ArduinoOTA.setHostname(_otaHostname.c_str());
            }

            if (_otaPassword.isEmpty()) {
                ArduinoOTA.setPassword("ota");
            }
            else {
                ArduinoOTA.setPassword(_otaPassword.c_str());
            }

            ArduinoOTA.begin();
            _otaInitialized = true;

        if (_otaInitialized) {
            ArduinoOTA.handle();  // This is crucial for handling requests
            return;
        }


        }else {
            // log_message("OTA: Waiting for WiFi connection...");
        }
    }

    void stopOTA() {
        if (_otaInitialized) {
            // TODO: There is no official ArduinoOTA stop API. If future versions expose one, replace this placeholder.
            ArduinoOTA.end(); // Placeholder (currently not part of core ArduinoOTA)
            _otaInitialized = false;
            log_message("🛜 OTA Stopped");
        }
        _otaEnabled = false;
    }

    void checkSettingsForErrors() {
        for (auto *s : settings) {
            if (s->hasError()) {
                log_message("[ERROR] Setting %s/%s has errors: %s",
                        s->getCategory(), s->getName(), s->getError());
            }
        }
    }

    private:
        Preferences prefs;
        std::vector<BaseSetting *> settings;
        
    // ---- Runtime (non-persistent) value providers (always enabled) ----
    public:
        struct RuntimeValueProvider {
            String name;                 // object name in JSON root
            std::function<void(JsonObject&)> fill; // called to populate nested object
            int order = 100;             // ordering weight (lower first)
        };

        struct RuntimeFieldStyleProperty {
            String name;
            String value;
        };

        struct RuntimeFieldStyleRule {
            String key;
            bool hasVisible = false;
            bool visible = true;
            std::vector<RuntimeFieldStyleProperty> properties;

            RuntimeFieldStyleRule& setVisible(bool v){
                hasVisible = true;
                visible = v;
                return *this;
            }

            RuntimeFieldStyleRule& set(const String& property, const String& val){
                for(auto &p : properties){
                    if(p.name == property){
                        p.value = val;
                        return *this;
                    }
                }
                properties.push_back({property, val});
                return *this;
            }
        };

        struct RuntimeFieldStyle {
            std::vector<RuntimeFieldStyleRule> rules;

            RuntimeFieldStyleRule& rule(const String& name){
                for(auto &r : rules){
                    if(r.key == name) return r;
                }
                RuntimeFieldStyleRule entry;
                entry.key = name;
                rules.push_back(entry);
                return rules.back();
            }

            const RuntimeFieldStyleRule* find(const String& name) const {
                for(const auto &r : rules){
                    if(r.key == name) return &r;
                }
                return nullptr;
            }

            bool empty() const { return rules.empty(); }

            void merge(const RuntimeFieldStyle& other){
                for(const auto &r : other.rules){
                    RuntimeFieldStyleRule &target = rule(r.key);
                    if(r.hasVisible){
                        target.hasVisible = true;
                        target.visible = r.visible;
                    }
                    for(const auto &prop : r.properties){
                        bool replaced = false;
                        for(auto &existing : target.properties){
                            if(existing.name == prop.name){
                                existing.value = prop.value;
                                replaced = true;
                                break;
                            }
                        }
                        if(!replaced){
                            target.properties.push_back(prop);
                        }
                    }
                }
            }
        };

        struct RuntimeFieldMeta {
            String group;        // provider name (e.g. sensors/system)
            String key;          // field key inside provider object
            String label;        // human readable label
            String unit;         // optional unit (e.g. °C, %, dBm)
            int precision = 1;   // decimals for floats
            bool isBool = false; // render as boolean indicator
            bool isButton = false; // interactive stateless button
            bool isCheckbox = false; // interactive two-way checkbox
            bool isStateButton = false; // stateful on/off style button (momentary toggle) without persistence
            bool isIntSlider = false;   // transient int slider control
            bool isFloatSlider = false; // transient float slider control
            // Slider config
            int intMin = 0; int intMax = 100; int intInit = 0; // for int slider
            float floatMin = 0.0f; float floatMax = 1.0f; float floatInit = 0.0f; int floatPrecision = 2; // for float slider
            // State button current state hint (reported in runtime JSON)
            bool initialState = false;
            String card;         // optional custom card assignment (UI grouping)
            // Optional threshold ranges (for numeric coloring)
            bool hasWarnMin = false; float warnMin = 0;
            bool hasWarnMax = false; float warnMax = 0;
            bool hasAlarmMin = false; float alarmMin = 0;
            bool hasAlarmMax = false; float alarmMax = 0;
            // 2025.09.29 Extensions:
            bool isString = false;    // show raw string value (no numeric formatting)
            String staticString;      // optional fixed string content (if not from runtime.json)
            bool isDivider = false;   // UI should render a visual separator / heading
            int order = 100;          // relative ordering inside group (lower first)
            int8_t boolAlarmValue = -1; // -1 -> no alarm semantics, 1 -> true means alarm
            RuntimeFieldStyle style;
        };

        // Feature flags / theming helpers
        bool _disableRuntimeStyleMeta = false; // if true, omit style objects from runtime_meta
        const char* _customCss = nullptr;      // pointer to optional globally injected CSS (not persisted)
        size_t _customCssLen = 0;              // optional length (0 -> strlen)
    public:
        void disableRuntimeStyleMeta(bool v){ _disableRuntimeStyleMeta = v; }
        bool isRuntimeStyleMetaDisabled() const { return _disableRuntimeStyleMeta; }
        // Provide a pointer to a compile-time or static CSS string. Not copied -> keep storage alive.
        void setCustomCss(const char* css, size_t len = 0){ _customCss = css; _customCssLen = len; }
        const char* getCustomCss() const { return _customCss; }
        size_t getCustomCssLen() const { return _customCss ? (_customCssLen? _customCssLen : strlen(_customCss)) : 0; }

        static RuntimeFieldStyle defaultNumericStyle(bool hasUnit = true){
            RuntimeFieldStyle style;
            style.rule("label").set("fontWeight", "600");

            RuntimeFieldStyleRule &valueRule = style.rule("values");
            valueRule.set("fontWeight", "600");
            valueRule.set("textAlign", "right");
            valueRule.set("fontVariantNumeric", "tabular-nums");

            RuntimeFieldStyleRule &unitRule = style.rule("unit");
            if(hasUnit){
            } else {
                unitRule.setVisible(false);
            }
            return style;
        }

        static RuntimeFieldStyle defaultBoolStyle(bool alarmWhenTrue = false){
            RuntimeFieldStyle style;
            style.rule("label").set("fontWeight", "600");

            RuntimeFieldStyleRule &stateRule = style.rule("state");
            stateRule.set("fontWeight", "600");
            stateRule.set("textAlign", "right");

            RuntimeFieldStyleRule &unitRule = style.rule("unit");
            unitRule.setVisible(false);

            RuntimeFieldStyleRule &dotTrue = style.rule("stateDotOnTrue");
            dotTrue.setVisible(true);
            dotTrue.set("background", "#2ecc71");
            dotTrue.set("border", "none");
            dotTrue.set("boxShadow", "0 0 2px rgba(0,0,0,0.4)");

            RuntimeFieldStyleRule &dotFalse = style.rule("stateDotOnFalse");
            dotFalse.setVisible(true);
            dotFalse.set("background", "#888");
            dotFalse.set("border", "1px solid #000");
            dotFalse.set("boxShadow", "0 0 2px rgba(0,0,0,0.4)");

            if(alarmWhenTrue){
                dotFalse.set("background", "#2ecc71");
                dotFalse.set("border", "none");
                RuntimeFieldStyleRule &dotAlarm = style.rule("stateDotOnAlarm");
                dotAlarm.setVisible(true);
                dotAlarm.set("background", "#d00000");
                dotAlarm.set("animation", "blink 1.6s linear infinite");
                dotAlarm.set("boxShadow", "0 0 4px rgba(208,0,0,0.7)");
            }
            return style;
        }

        static RuntimeFieldStyle defaultStringStyle(){
            RuntimeFieldStyle style;
            style.rule("label").set("fontWeight", "600");

            RuntimeFieldStyleRule &valueRule = style.rule("values");
            valueRule.set("fontWeight", "500");
            valueRule.set("textAlign", "left");

            RuntimeFieldStyleRule &unitRule = style.rule("unit");
            unitRule.setVisible(false);
            return style;
        }

        // Backward-compatible minimal definition
        void defineRuntimeField(const String &group,
                                const String &key,
                                const String &label,
                                const String &unit = String(),
                                int precision = 1,
                                int order = 100,
                                const RuntimeFieldStyle& styleOverride = RuntimeFieldStyle()){
            RuntimeFieldMeta m;
            m.group = group;
            m.key = key;
            m.label = label;
            m.unit = unit;
            m.precision = precision;
            m.order = order;
            m.style = defaultNumericStyle(unit.length() > 0);
            if(!styleOverride.empty()){
                m.style.merge(styleOverride);
            }
            _runtimeMeta.push_back(m);
        }
        // Extended with thresholds (pass NAN or omit to skip)
        void defineRuntimeFieldThresholds(
            const String &group, const String &key, const String &label, const String &unit,
            int precision,
            float warnMin, float warnMax,
            float alarmMin, float alarmMax,
            bool enableWarnMin = true, bool enableWarnMax = true,
            bool enableAlarmMin = true, bool enableAlarmMax = true,
            int order = 100,
            const RuntimeFieldStyle& styleOverride = RuntimeFieldStyle()
        ){
            RuntimeFieldMeta m; m.group=group; m.key=key; m.label=label; m.unit=unit; m.precision=precision; m.order=order;
            if(enableWarnMin){ m.hasWarnMin = true; m.warnMin = warnMin; }
            if(enableWarnMax){ m.hasWarnMax = true; m.warnMax = warnMax; }
            if(enableAlarmMin){ m.hasAlarmMin = true; m.alarmMin = alarmMin; }
            if(enableAlarmMax){ m.hasAlarmMax = true; m.alarmMax = alarmMax; }
            m.style = defaultNumericStyle(unit.length() > 0);
            if(!styleOverride.empty()){
                m.style.merge(styleOverride);
            }
            _runtimeMeta.push_back(m);
        }
    // Define a boolean runtime value. Set alarmWhenTrue to enable alarm semantics (true state treated as alarm).
        void defineRuntimeBool(const String &group,
                               const String &key,
                               const String &label,
                               bool alarmWhenTrue=false,
                               int order = 100,
                               const RuntimeFieldStyle& styleOverride = RuntimeFieldStyle()){
            RuntimeFieldMeta m;
            m.group = group;
            m.key = key;
            m.label = label;
            m.isBool = true;
            m.order = order;
            if(alarmWhenTrue){ m.boolAlarmValue = 1; }
            m.style = defaultBoolStyle(alarmWhenTrue);
            if(!styleOverride.empty()){
                m.style.merge(styleOverride);
            }
            _runtimeMeta.push_back(m);
        }

        // New: string (non-numeric, non-boolean) runtime label & value. If key is empty, acts as standalone display entry.
        void defineRuntimeString(const String &group,
                                 const String &key,
                                 const String &label,
                                 const String &staticValue = String(),
                                 int order = 100,
                                 const RuntimeFieldStyle& styleOverride = RuntimeFieldStyle()){
            RuntimeFieldMeta m;
            m.group = group;
            m.key = key;
            m.label = label;
            m.isString = true;
            m.order = order;
            m.staticString = staticValue;
            m.style = defaultStringStyle();
            if(!styleOverride.empty()){
                m.style.merge(styleOverride);
            }
            _runtimeMeta.push_back(m);
        }
        // New: divider/separator inside a group. Key becomes unique synthetic id (e.g., "_div_<n>").
        void defineRuntimeDivider(const String &group, const String &label, int order = 100){
            static int dividerCounter = 0; // local static counter
            RuntimeFieldMeta m; m.group=group; m.key = String("_div_") + String(++dividerCounter); m.label=label; m.isDivider=true; m.order=order; _runtimeMeta.push_back(m);
        }

        // Interactive stateless button
        void defineRuntimeButton(const String &group,
                                 const String &key,
                                 const String &label,
                                 std::function<void()> onPress,
                                 int order = 100,
                                 const RuntimeFieldStyle& styleOverride = RuntimeFieldStyle(),
                                 const String &card = String()){
            RuntimeFieldMeta m; m.group=group; m.key=key; m.label=label; m.isButton=true; m.order=order; m.precision=0; m.style = defaultStringStyle(); m.card = card;
            if(!styleOverride.empty()) m.style.merge(styleOverride);
            _runtimeMeta.push_back(m);
            _runtimeButtons.push_back({group,key,onPress});
        }

        // Interactive checkbox (toggle)
        void defineRuntimeCheckbox(const String &group,
                                   const String &key,
                                   const String &label,
                                   std::function<bool()> getter,
                                   std::function<void(bool)> setter,
                                   int order = 100,
                                   const RuntimeFieldStyle& styleOverride = RuntimeFieldStyle(),
                                   const String &card = String()){
            RuntimeFieldMeta m; m.group=group; m.key=key; m.label=label; m.isCheckbox=true; m.order=order; m.precision=0; m.style = defaultBoolStyle(false); m.card = card;
            m.style.rule("unit").setVisible(false);
            if(!styleOverride.empty()) m.style.merge(styleOverride);
            _runtimeMeta.push_back(m);
            _runtimeCheckboxes.push_back({group,key,getter,setter});
        }

        // Stateful runtime button (has on/off state; toggles and invokes callback)
        struct _StateButtonDef { String group; String key; std::function<bool()> getter; std::function<void(bool)> setter; };
        std::vector<_StateButtonDef> _runtimeStateButtons;
        void defineRuntimeStateButton(const String &group,
                                      const String &key,
                                      const String &label,
                                      std::function<bool()> getter,
                                      std::function<void(bool)> setter,
                                      bool initState = false,
                                      int order = 100,
                                      const RuntimeFieldStyle& styleOverride = RuntimeFieldStyle(),
                                      const String &card = String()){
            RuntimeFieldMeta m; m.group=group; m.key=key; m.label=label; m.isStateButton = true; m.order=order; m.initialState = initState; m.style = defaultBoolStyle(false); m.card = card; m.style.rule("unit").setVisible(false);
            if(!styleOverride.empty()) m.style.merge(styleOverride);
            _runtimeMeta.push_back(m);
            _runtimeStateButtons.push_back({group,key,getter,setter});
        }

        // Int slider (transient; not persisted)
        struct _IntSliderDef { String group; String key; std::function<int()> getter; std::function<void(int)> setter; int minV; int maxV; };
        std::vector<_IntSliderDef> _runtimeIntSliders;
        void defineRuntimeIntSlider(const String &group,
                                    const String &key,
                                    const String &label,
                                    int minValue, int maxValue, int initValue,
                                    std::function<int()> getter,
                                    std::function<void(int)> setter,
                                    int order = 100,
                                    const RuntimeFieldStyle& styleOverride = RuntimeFieldStyle(),
                                    const String &card = String()){
            RuntimeFieldMeta m; m.group=group; m.key=key; m.label=label; m.isIntSlider=true; m.order=order; m.intMin=minValue; m.intMax=maxValue; m.intInit=initValue; m.card = card; m.style = defaultNumericStyle();
            if(!styleOverride.empty()) m.style.merge(styleOverride);
            _runtimeMeta.push_back(m);
            _runtimeIntSliders.push_back({group,key,getter,setter,minValue,maxValue});
        }

        // Float slider (transient; not persisted)
        struct _FloatSliderDef { String group; String key; std::function<float()> getter; std::function<void(float)> setter; float minV; float maxV; };
        std::vector<_FloatSliderDef> _runtimeFloatSliders;
        void defineRuntimeFloatSlider(const String &group,
                                      const String &key,
                                      const String &label,
                                      float minValue, float maxValue, float initValue, int precision,
                                      std::function<float()> getter,
                                      std::function<void(float)> setter,
                                      int order = 100,
                                      const RuntimeFieldStyle& styleOverride = RuntimeFieldStyle(),
                                      const String &card = String()){
            RuntimeFieldMeta m; m.group=group; m.key=key; m.label=label; m.isFloatSlider=true; m.order=order; m.floatMin=minValue; m.floatMax=maxValue; m.floatInit=initValue; m.precision = precision; m.floatPrecision = precision; m.card = card; m.style = defaultNumericStyle();
            if(!styleOverride.empty()) m.style.merge(styleOverride);
            _runtimeMeta.push_back(m);
            _runtimeFloatSliders.push_back({group,key,getter,setter,minValue,maxValue});
        }

        // Set / override provider order
        void setRuntimeProviderOrder(const String &provider, int order){
            for(auto &p : runtimeProviders){ if(p.name == provider){ p.order = order; return; } }
        }

        void addRuntimeProvider(const RuntimeValueProvider &p){ runtimeProviders.push_back(p); }

        String runtimeValuesToJSON(){
            DynamicJsonDocument d(2048);
            JsonObject root = d.to<JsonObject>();
            root["uptime"] = millis();
            // Sort providers by order
            std::vector<RuntimeValueProvider> provSorted = runtimeProviders;
            std::sort(provSorted.begin(), provSorted.end(), [](const RuntimeValueProvider &a, const RuntimeValueProvider &b){ return a.order < b.order; });
            for(auto &prov : provSorted){
                JsonObject slot = root.createNestedObject(prov.name);
                if (prov.fill) prov.fill(slot);
                // Inject checkbox states
                for(const auto &cbx : _runtimeCheckboxes){ if(cbx.group == prov.name && !slot.containsKey(cbx.key.c_str()) && cbx.getter){ slot[cbx.key] = cbx.getter(); } }
                // Inject state buttons
                for(const auto &sb : _runtimeStateButtons){ if(sb.group == prov.name && !slot.containsKey(sb.key.c_str()) && sb.getter){ slot[sb.key] = sb.getter(); } }
                // Inject int sliders
                for(const auto &is : _runtimeIntSliders){ if(is.group == prov.name && !slot.containsKey(is.key.c_str()) && is.getter){ slot[is.key] = is.getter(); } }
                // Inject float sliders
                for(const auto &fs : _runtimeFloatSliders){ if(fs.group == prov.name && !slot.containsKey(fs.key.c_str()) && fs.getter){ slot[fs.key] = fs.getter(); } }
            }
            if(!_runtimeAlarms.empty()){
                JsonObject alarms = root.createNestedObject("alarms");
                for(auto &a : _runtimeAlarms){ alarms[a.name] = a.active; }
            }
            String out; serializeJson(d, out); return out;
        }
        String runtimeMetaToJSON(){
            DynamicJsonDocument d(4096);
            JsonArray arr = d.to<JsonArray>();
            // Sort by group, then order, then label
            std::vector<RuntimeFieldMeta> metaSorted;
#ifdef development
            if(_runtimeMetaOverrideActive) metaSorted = _runtimeMetaOverride; else metaSorted = _runtimeMeta;
#else
            metaSorted = _runtimeMeta;
#endif
            std::sort(metaSorted.begin(), metaSorted.end(), [](const RuntimeFieldMeta &a, const RuntimeFieldMeta &b){
                if(a.group == b.group){ if(a.order == b.order) return a.label < b.label; return a.order < b.order; }
                return a.group < b.group;
            });
            for(auto &m : metaSorted){
                JsonObject o = arr.createNestedObject();
                o["group"] = m.group;
                o["key"] = m.key;
                o["label"] = m.label;
                if(m.unit.length()) o["unit"] = m.unit;
                o["precision"] = m.precision;
                if(m.isBool) o["isBool"] = true;
                if(m.isButton) o["isButton"] = true;
                if(m.isCheckbox) o["isCheckbox"] = true;
                if(m.isStateButton) { o["isStateButton"] = true; o["initState"] = m.initialState; }
                if(m.isIntSlider){
                    o["isIntSlider"] = true; o["min"] = m.intMin; o["max"] = m.intMax; o["init"] = m.intInit;
                }
                if(m.isFloatSlider){
                    o["isFloatSlider"] = true; o["min"] = m.floatMin; o["max"] = m.floatMax; o["init"] = m.floatInit; o["precision"] = m.floatPrecision;
                }
                if(m.isString) o["isString"] = true;
                if(m.isDivider) o["isDivider"] = true;
                if(m.staticString.length()) o["staticValue"] = m.staticString;
                if(m.order != 100) o["order"] = m.order;
                if(m.card.length()) o["card"] = m.card;
                // include thresholds only if defined to keep payload compact
                if(m.hasWarnMin) o["warnMin"] = m.warnMin;
                if(m.hasWarnMax) o["warnMax"] = m.warnMax;
                if(m.hasAlarmMin) o["alarmMin"] = m.alarmMin;
                if(m.hasAlarmMax) o["alarmMax"] = m.alarmMax;
                if(m.boolAlarmValue != -1){
                    o["boolAlarmValue"] = (m.boolAlarmValue == 1);
                }
                if(!_disableRuntimeStyleMeta && !m.style.empty()){
                    JsonObject styleObj = o.createNestedObject("style");
                    for(const auto &rule : m.style.rules){
                        if(!rule.key.length()) continue;
                        JsonObject ruleObj = styleObj.createNestedObject(rule.key);
                        if(rule.hasVisible){ ruleObj["visible"] = rule.visible; }
                        for(const auto &prop : rule.properties){
                            if(prop.name.length()) ruleObj[prop.name.c_str()] = prop.value;
                        }
                    }
                }
            }
            String out; serializeJson(d, out); return out;
        }
    private:
        std::vector<RuntimeValueProvider> runtimeProviders;
    std::vector<RuntimeFieldMeta> _runtimeMeta;
#ifdef development
    std::vector<RuntimeFieldMeta> _runtimeMetaOverride; // injected via /runtime_meta/override
    bool _runtimeMetaOverrideActive = false;
#endif
        struct RuntimeButton { String group; String key; std::function<void()> onPress; };
        struct RuntimeCheckbox { String group; String key; std::function<bool()> getter; std::function<void(bool)> setter; };
        std::vector<RuntimeButton> _runtimeButtons;
        std::vector<RuntimeCheckbox> _runtimeCheckboxes;
        // Cross-field runtime alarms
        struct RuntimeAlarm {
            String name; // identifier
            std::function<bool(const JsonObject&)> condition; // receives merged runtime JSON root
            std::function<void()> onEnter; // called when condition becomes true
            std::function<void()> onExit;  // called when condition leaves true
            bool active = false; // current state
        };
        std::vector<RuntimeAlarm> _runtimeAlarms;
    public:
        void defineRuntimeAlarm(const String &name,
                                std::function<bool(const JsonObject&)> condition,
                                std::function<void()> onEnter = nullptr,
                                std::function<void()> onExit = nullptr){
            _runtimeAlarms.push_back({name,condition,onEnter,onExit,false});
        }
        void handleRuntimeAlarms(){
            if(_runtimeAlarms.empty()) return;
            // Build a transient runtime JSON document (reuse existing builder without string serialize)
            DynamicJsonDocument d(768);
            JsonObject root = d.to<JsonObject>();
            root["uptime"] = millis();
            for(auto &prov : runtimeProviders){
                JsonObject slot = root.createNestedObject(prov.name);
                if (prov.fill) prov.fill(slot);
            }
            for(auto &al : _runtimeAlarms){
                bool cond = false;
                if(al.condition){
                    try { cond = al.condition(root); } catch(...){ cond = false; }
                }
                if(cond && !al.active){
                    al.active = true; if(al.onEnter) al.onEnter();
                } else if(!cond && al.active){
                    al.active = false; if(al.onExit) al.onExit();
                }
            }
        }
    private:
        AsyncWebSocket* _ws = nullptr;
        bool _wsInitialized = false;
        bool _wsEnabled = false;
        bool _pushOnConnect = true;
        uint32_t _wsInterval = 2000;
        unsigned long _wsLastPush = 0;
        std::function<String()> _customPayloadBuilder;
    public:
        WebHTML webhtml;
        static LogCallback logger;

        bool _otaEnabled = false;
        bool _otaInitialized = false;
        String _otaPassword;
        String _otaHostname;
    String _appName;
    // Built-in system provider support
    bool _builtinSystemProviderEnabled = false;
    bool _builtinSystemProviderRegistered = false;
    // cached loop average (if user wants to push their own they still can redefine) - user code can update via updateLoopAvg
    double _loopAvgMs = 0.0;
    unsigned long _loopWindowStart = 0;
    uint32_t _loopSamples = 0;
    double _loopAccumMs = 0.0;

public:
    // Call from loop() to update rolling average (3s window) if builtin system provider is enabled.
    void updateLoopTiming(){
        if(!_builtinSystemProviderEnabled) return;
        static unsigned long lastMicros = micros();
        unsigned long nowMicros = micros();
        unsigned long delta = nowMicros - lastMicros; // overflow safe unsigned arithmetic
        lastMicros = nowMicros;
        double ms = (double)delta / 1000.0;
        _loopAccumMs += ms;
        _loopSamples++;
        unsigned long nowMs = millis();
        if(_loopWindowStart == 0) _loopWindowStart = nowMs;
        if(nowMs - _loopWindowStart >= 3000){
            if(_loopSamples > 0){
                _loopAvgMs = _loopAccumMs / (double)_loopSamples;
            }
            _loopAccumMs = 0.0;
            _loopSamples = 0;
            _loopWindowStart = nowMs;
        }
    }

    // Simple RSSI quality to emoji (bars) mapping
    static const char* rssiEmoji(int rssi){
        //todo:search for better svg or emoji for
        // Typical RSSI: -30 (excellent) to -90 (bad)
        if(rssi >= -50) return "Excellent"; // Excellent
        if(rssi >= -60) return "Good"; // Good
        if(rssi >= -67) return "Fair"; // Fair
        if(rssi >= -75) return "Weak"; // Weak
        return "Very weak / none"; // Very weak / none
    }

    // Enable built-in system provider (must be called in setup before starting web server)
    void enableBuiltinSystemProvider(bool alsoDefineMeta = true){
        if(_builtinSystemProviderEnabled) return; // idempotent
        _builtinSystemProviderEnabled = true;
        if(!_builtinSystemProviderRegistered){
            addRuntimeProvider({
                .name = "system",
                .fill = [this](JsonObject &o){
                    o["rssi"] = WiFi.isConnected() ? WiFi.RSSI() : 0;
                    o["rssiBars"] = rssiEmoji(WiFi.isConnected()? WiFi.RSSI(): -100);
                    o["freeHeap"] = (uint32_t)(ESP.getFreeHeap()/1024); // KB
                    o["loopAvg"] = _loopAvgMs; // ms avg over last window
                },
                .order = 0
            });
            if(alsoDefineMeta){
                // Order values chosen to interleave nicely; user can override later.
                defineRuntimeField("system","rssi","WiFi RSSI","dBm",0,1);
                defineRuntimeString("system","rssiBars","Signal","",2); // will show string bars
                defineRuntimeField("system","freeHeap","Free Heap","KB",0,3);
                defineRuntimeField("system","loopAvg","Loop Avg","ms",2,4);
            }
            _builtinSystemProviderRegistered = true;
        }
    }

    bool isBuiltinSystemProviderEnabled() const { return _builtinSystemProviderEnabled; }

};



// extern ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr;
extern ConfigManagerClass ConfigManager;