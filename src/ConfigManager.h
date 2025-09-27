#include <Arduino.h>
#include <string_view>
#include <Preferences.h>
#include <vector>
#include <functional>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <type_traits>
#ifdef USE_ASYNC_WEBSERVER
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#ifdef ENABLE_WEBSOCKET_PUSH
#include <ArduinoJson.h>
#endif
#else
#include <WebServer.h>
#endif
#include <exception>
#include <ArduinoOTA.h>
#include <Update.h>  // Required for U_FLASH
#include "html_content.h" // NOTE: WEB_HTML is now generated from webui (Vue project). Build webui and copy dist/index.html here as a string literal.

#ifdef UNIT_TEST
// Inline fallback to ensure linker finds implementation during unit tests even if html_content.cpp filtered out
inline const char* WebHTML::getWebHTML() { return WEB_HTML; }
#endif

#define CONFIGMANAGER_VERSION "2.4.0"


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
#pragma once


// Server abstraction
#ifdef USE_ASYNC_WEBSERVER
extern AsyncWebServer server; // defined in main when async mode
#else
extern WebServer server;
#endif
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

class ConfigManagerClass {

public:

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

    void handleClient() {
#ifndef USE_ASYNC_WEBSERVER
        server.handleClient();
#endif
    }

#ifdef ENABLE_WEBSOCKET_PUSH
    // Call frequently from loop() in async builds
    void handleWebsocketPush(){
        if(!_wsEnabled) return;
        unsigned long now = millis();
        if (now - _wsLastPush < _wsInterval) return;
        _wsLastPush = now;
        pushRuntimeNow();
    }
    void enableWebSocketPush(uint32_t intervalMs = 2000){
#ifdef USE_ASYNC_WEBSERVER
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
#endif
    }
    void disableWebSocketPush(){ _wsEnabled = false; }
    void setWebSocketInterval(uint32_t intervalMs){ _wsInterval = intervalMs; }
    void setPushOnConnect(bool v){ _pushOnConnect = v; }
    // Optional external payload hook
    void setCustomLivePayloadBuilder(std::function<String()> fn){ _customPayloadBuilder = fn; }
    void pushRuntimeNow(){
#ifdef USE_ASYNC_WEBSERVER
        if(!_wsInitialized || !_wsEnabled) return;
        String payload;
        if(_customPayloadBuilder){ payload = _customPayloadBuilder(); }
        else {
#ifdef ENABLE_LIVE_VALUES
            payload = runtimeValuesToJSON();
#else
            // minimal fallback
            DynamicJsonDocument d(128); d["uptime"]=millis(); serializeJson(d, payload);
#endif
        }
        _ws->textAll(payload);
#endif
    }
#endif // ENABLE_WEBSOCKET_PUSH

    void handleClientSecure() {
    // Only meaningful for sync (blocking) WebServer variant
#ifndef USE_ASYNC_WEBSERVER
    server.handleClient();
#endif
    // secureServer->handleClient(); (future TLS hook)
    }

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

    // Serve version at /version
#ifdef USE_ASYNC_WEBSERVER
    server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", CONFIGMANAGER_VERSION);
    });
#else
    server.on("/version", HTTP_GET, []() {
        server.send(200, "text/plain", CONFIGMANAGER_VERSION);
    });
#endif

        // Legacy HTTP OTA upload only supported in sync server variant
#ifndef USE_ASYNC_WEBSERVER
        server.on("/ota_update", HTTP_GET, [this]() {
            String html = R"(
<!DOCTYPE html><html><body><h2>OTA Update (Sync)</h2>
<form method='POST' action='/ota_update' enctype='multipart/form-data'>
<input type='file' keyName='firmware'><input type='submit' value='Update'>
</form></body></html>)";
            server.send(200, "text/html", html);
        });
        server.on("/ota_update", HTTP_POST, [this]() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", Update.hasError() ? "UPDATE FAILED" : "UPDATE SUCCESS");
            ESP.restart();
        }, [this]() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                log_message("Update: %s", upload.filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) { Update.printError(Serial); }
            } else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) { log_message("Update Success: rebooting"); } else { Update.printError(Serial); }
            }
        });
#else
        server.on("/ota_update", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "HTTP OTA form only available in sync build. Use ArduinoOTA / network upload.");
        });
#endif

    // Reset to Defaults
#ifdef USE_ASYNC_WEBSERVER
    server.on("/config/reset", HTTP_POST, [this](AsyncWebServerRequest *request){
        for (auto *s : settings) s->setDefault();
        saveAll();
        log_message("🌐 All settings reset to default");
        request->send(200, "application/json", "{\"status\":\"reset\"}");
    });
#else
    server.on("/config/reset", HTTP_POST, [this]() {
        for (auto *s : settings) s->setDefault();
        saveAll();
        log_message("🌐 All settings reset to default");
        server.send(200, "application/json", "{\"status\":\"reset\"}");
    });
#endif

        // Apply single setting
#ifdef USE_ASYNC_WEBSERVER
        server.on("/config/apply", HTTP_POST,
            [this](AsyncWebServerRequest *request){ /* response sent in body handler */ },
            NULL,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                if(index == 0){ request->_tempObject = new String(); static_cast<String*>(request->_tempObject)->reserve(total); }
                String *body = static_cast<String*>(request->_tempObject);
                body->concat(String((const char*)data).substring(0,len));
                if(index + len == total){
                    String category = request->getParam("category", true)->value();
                    String key = request->getParam("key", true)->value();
                    log_message("🌐 Apply: %s/%s", category.c_str(), key.c_str());
                    DynamicJsonDocument doc(256);
                    DeserializationError err = deserializeJson(doc, *body);
                    auto isIntegerString = [](const String &s)->bool { if(!s.length()) return false; int st=0; if(s[0]=='-'&&s.length()>1) st=1; for(int i=st;i<(int)s.length();++i){ if(s[i]<'0'||s[i]>'9') return false;} return true; };
                    JsonVariant val;
                    if(!err){ if(doc.containsKey("value")) val = doc["value"]; else val = doc.as<JsonVariant>(); }
                    else { doc.clear(); if(body->equalsIgnoreCase("true")||body->equalsIgnoreCase("false")) doc["value"] = body->equalsIgnoreCase("true"); else if(isIntegerString(*body)) doc["value"] = body->toInt(); else doc["value"] = *body; val = doc["value"]; }
                    for(auto *s: settings){ if(String(s->getCategory())==category && String(s->getName())==key){ if(s->fromJSON(val)){ request->send(200, "application/json", "{\"status\":\"applied\"}"); delete body; request->_tempObject=nullptr; return;} else { request->send(400, "application/json", "{\"status\":\"invalid_value\"}"); delete body; request->_tempObject=nullptr; return;} } }
                    request->send(404, "application/json", "{\"status\":\"not_found\"}"); delete body; request->_tempObject=nullptr;
                }
            }
        );
#else
        server.on("/config/apply", HTTP_POST, [this]() {
            String category = server.arg("category");
            String key = server.arg("key");
            log_message("🌐 Apply: %s/%s\n", category.c_str(), key.c_str());
            String body = server.arg("plain");
            if (body.isEmpty() && server.args() > 0) body = server.arg(server.args()-1);
            DynamicJsonDocument doc(256);
            DeserializationError err = deserializeJson(doc, body);
            auto isIntegerString = [](const String &s)->bool { if(!s.length()) return false; int st=0; if(s[0]=='-'&&s.length()>1) st=1; for(int i=st;i<(int)s.length();++i){ if(s[i]<'0'||s[i]>'9') return false;} return true; };
            JsonVariant val;
            if(!err){ if(doc.containsKey("value")) val = doc["value"]; else val = doc.as<JsonVariant>(); }
            else { doc.clear(); if(body.equalsIgnoreCase("true")||body.equalsIgnoreCase("false")) doc["value"] = body.equalsIgnoreCase("true"); else if(isIntegerString(body)) doc["value"] = body.toInt(); else doc["value"] = body; val = doc["value"]; }
            for(auto *s: settings){ if(String(s->getCategory())==category && String(s->getName())==key){ if(s->fromJSON(val)){ server.send(200, "application/json", "{\"status\":\"applied\"}"); return;} else { server.send(400, "application/json", "{\"status\":\"invalid_value\"}"); return;} } }
            server.send(404, "application/json", "{\"status\":\"not_found\"}");
        });
#endif

        // Apply all
#ifdef USE_ASYNC_WEBSERVER
        server.on("/config/apply_all", HTTP_POST,
            [this](AsyncWebServerRequest *request){}, NULL,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                if(index==0){ request->_tempObject = new String(); static_cast<String*>(request->_tempObject)->reserve(total); }
                String *body = static_cast<String*>(request->_tempObject);
                body->concat(String((const char*)data).substring(0,len));
                if(index+len==total){
                    DynamicJsonDocument doc(1024);
                    deserializeJson(doc, *body);
                    if(fromJSON(doc)) request->send(200, "application/json", "{\"status\":\"applied\"}"); else request->send(400, "application/json", "{\"status\":\"invalid\"}");
                    delete body; request->_tempObject=nullptr;
                }
            }
        );
#else
        server.on("/config/apply_all", HTTP_POST, [this]() {
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, server.arg("plain"));
            if (fromJSON(doc)) server.send(200, "application/json", "{\"status\":\"applied\"}"); else server.send(400, "application/json", "{\"status\":\"invalid\"}");
        });
#endif

        // Save single setting
#ifdef USE_ASYNC_WEBSERVER
        server.on("/config/save", HTTP_POST,
            [this](AsyncWebServerRequest *request){}, NULL,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                if(index==0){ request->_tempObject = new String(); static_cast<String*>(request->_tempObject)->reserve(total);} 
                String *body = static_cast<String*>(request->_tempObject);
                body->concat(String((const char*)data).substring(0,len));
                if(index+len==total){
                    String category = request->getParam("category", true)->value();
                    String key = request->getParam("key", true)->value();
                    DynamicJsonDocument doc(256);
                    DeserializationError err = deserializeJson(doc, *body);
                    auto isIntegerString = [](const String &s)->bool { if(!s.length()) return false; int st=0; if(s[0]=='-'&&s.length()>1) st=1; for(int i=st;i<(int)s.length();++i){ if(s[i]<'0'||s[i]>'9') return false;} return true; };
                    JsonVariant val; if(!err){ if(doc.containsKey("value")) val = doc["value"]; else val = doc.as<JsonVariant>(); } else { doc.clear(); if(body->equalsIgnoreCase("true")||body->equalsIgnoreCase("false")) doc["value"] = body->equalsIgnoreCase("true"); else if(isIntegerString(*body)) doc["value"] = body->toInt(); else doc["value"] = *body; val = doc["value"]; }
                    for(auto *s: settings){ if(String(s->getCategory())==category && String(s->getName())==key){ if(s->fromJSON(val)){ prefs.begin("config", false); s->save(prefs); prefs.end(); request->send(200, "application/json", "{\"status\":\"saved\"}"); delete body; request->_tempObject=nullptr; return;} else { request->send(400, "application/json", "{\"status\":\"invalid_value\"}"); delete body; request->_tempObject=nullptr; return;} } }
                    request->send(404, "application/json", "{\"status\":\"not_found\"}"); delete body; request->_tempObject=nullptr;
                }
            }
        );
#else
        server.on("/config/save", HTTP_POST, [this]() {
            String category = server.arg("category");
            String key = server.arg("key");
            String body = server.arg("plain");
            if (body.isEmpty() && server.args()>0) body = server.arg(server.args()-1);
            DynamicJsonDocument doc(256); DeserializationError err = deserializeJson(doc, body);
            auto isIntegerString = [](const String &s)->bool { if(!s.length()) return false; int st=0; if(s[0]=='-'&&s.length()>1) st=1; for(int i=st;i<(int)s.length();++i){ if(s[i]<'0'||s[i]>'9') return false;} return true; };
            JsonVariant val; if(!err){ if(doc.containsKey("value")) val=doc["value"]; else val=doc.as<JsonVariant>(); } else { doc.clear(); if(body.equalsIgnoreCase("true")||body.equalsIgnoreCase("false")) doc["value"] = body.equalsIgnoreCase("true"); else if(isIntegerString(body)) doc["value"] = body.toInt(); else doc["value"] = body; val = doc["value"]; }
            for(auto *s: settings){ if(String(s->getCategory())==category && String(s->getName())==key){ if(s->fromJSON(val)){ prefs.begin("config", false); s->save(prefs); prefs.end(); server.send(200, "application/json", "{\"status\":\"saved\"}"); return; } else { server.send(400, "application/json", "{\"status\":\"invalid_value\"}"); return; } } }
            server.send(404, "application/json", "{\"status\":\"not_found\"}");
        });
#endif

    // Reboot
#ifdef USE_ASYNC_WEBSERVER
    server.on("/config/reboot", HTTP_POST, [this](AsyncWebServerRequest *request){
        request->send(200, "application/json", "{\"status\":\"rebooting\"}");
        log_message("🔄 Device rebooting...");
        // Schedule reboot after short delay (simple blocking delay acceptable here)
        delay(100);
        reboot();
    });
#else
    server.on("/config/reboot", HTTP_POST, [this]() {
        server.send(200, "application/json", "{\"status\":\"rebooting\"}");
        log_message("🔄 Device rebooting...");
        delay(100);
        reboot();
    });
#endif

        // Config JSON
    // Configuration dump
#ifdef USE_ASYNC_WEBSERVER
    server.on("/config.json", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(200, "application/json", toJSON());
    });
#else
    server.on("/config.json", HTTP_GET, [this]() {
        server.send(200, "application/json", toJSON());
    });
#endif

#ifdef ENABLE_LIVE_VALUES
    // Live runtime values (polling JSON)
#ifdef USE_ASYNC_WEBSERVER
    server.on("/runtime.json", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(200, "application/json", runtimeValuesToJSON());
    });
#else
    server.on("/runtime.json", HTTP_GET, [this]() {
        server.send(200, "application/json", runtimeValuesToJSON());
    });
#endif
#endif // ENABLE_LIVE_VALUES

        // Save all settings
#ifdef USE_ASYNC_WEBSERVER
        server.on("/config/save_all", HTTP_POST,
            [this](AsyncWebServerRequest *request){}, NULL,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                if(index==0){ request->_tempObject = new String(); static_cast<String*>(request->_tempObject)->reserve(total);} 
                String *body = static_cast<String*>(request->_tempObject);
                body->concat(String((const char*)data).substring(0,len));
                if(index+len==total){
                    DynamicJsonDocument doc(1024);
                    deserializeJson(doc, *body);
                    if(fromJSON(doc)){ saveAll(); request->send(200, "application/json", "{\"status\":\"saved\"}"); } else { request->send(400, "application/json", "{\"status\":\"invalid\"}"); }
                    delete body; request->_tempObject=nullptr;
                }
            }
        );
#else
        server.on("/config/save_all", HTTP_POST, [this]() {
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, server.arg("plain"));
            if (fromJSON(doc)) { saveAll(); server.send(200, "application/json", "{\"status\":\"saved\"}"); }
            else server.send(400, "application/json", "{\"status\":\"invalid\"}");
        });
#endif

    // Root (serves embedded SPA)
#ifdef USE_ASYNC_WEBSERVER
        server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send_P(200, "text/html", webhtml.getWebHTML());
        });
#else
        server.on("/", HTTP_GET, [this]() {
            server.send_P(200, "text/html", webhtml.getWebHTML());
        });
#endif

#ifdef USE_ASYNC_WEBSERVER
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
#else
        server.on("/", HTTP_POST, [this]() {
            log_message("🌐 Received POST request to root route");
            log_message("🌐 Body: %s", server.arg("plain"));
        });
#endif

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

            // (Optional debug) Uncomment if you need detailed WiFi info:
            // log_message("WiFi Status: %d", WiFi.status());
            // log_message("Local IP: %s", WiFi.localIP().toString().c_str());
            // log_message("Subnet Mask: %s", WiFi.subnetMask().toString().c_str());
            // log_message("Gateway IP: %s", WiFi.gatewayIP().toString().c_str());


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
                ArduinoOTA.setHostname("esp32-device");
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
            log_message("OTA: Waiting for WiFi connection...");
            // log_message("WiFi Status: %d\n", WiFi.status());
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
        
#ifdef ENABLE_LIVE_VALUES
        // ---- Runtime (non-persistent) value providers ----
    public:
        struct RuntimeValueProvider {
            String name; // object name in JSON root
            std::function<void(JsonObject&)> fill; // called to populate nested object
        };

        void addRuntimeProvider(const RuntimeValueProvider &p){ runtimeProviders.push_back(p); }

        String runtimeValuesToJSON(){
            DynamicJsonDocument d(768);
            JsonObject root = d.to<JsonObject>();
            root["uptime"] = millis();
            for(auto &prov : runtimeProviders){
                JsonObject slot = root.createNestedObject(prov.name);
                if (prov.fill) prov.fill(slot);
            }
            String out; serializeJson(d, out); return out;
        }
    private:
        std::vector<RuntimeValueProvider> runtimeProviders;
    public:
#endif
#ifdef ENABLE_WEBSOCKET_PUSH
    private:
        AsyncWebSocket* _ws = nullptr;
        bool _wsInitialized = false;
        bool _wsEnabled = false;
        bool _pushOnConnect = true;
        uint32_t _wsInterval = 2000;
        unsigned long _wsLastPush = 0;
        std::function<String()> _customPayloadBuilder;
    public:
#endif
        WebHTML webhtml;
        static LogCallback logger;

        bool _otaEnabled = false;
        bool _otaInitialized = false;
        String _otaPassword;
        String _otaHostname;

};



// extern ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr;
extern ConfigManagerClass ConfigManager;