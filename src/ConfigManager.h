#pragma once

// Settings are stored in the format: <category>_<key>

#include <Arduino.h>
#include <string_view>
#include <Preferences.h>
#include <vector>
#include <functional>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <exception>
#include <ArduinoOTA.h>
#include <Update.h>  // Required for U_FLASH
#include "html_content.h"

#define CONFIGMANAGER_VERSION "2.2.0"

extern WebServer server;
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
        const char *displayName; // 2025.08.17 - Add new feature for display keyName in web interface
        const char *categoryPretty = nullptr; // 2025.09.05 - Add pretty name for category
        SettingType type;
        bool hasKeyLengthError = false; //04.09.2025 bugfix: prevent an buffer overflow on to long category and / or (idk) have an white spaces in key or category.
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

    // 2025.08.17 - Add accessor for display keyName
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

    // 2025.08.17 - Updated constructor with display keyName
    // 2025.09.04 - New constructor with compile-time checking
    // 05.09.2025 - add new callback function with std::function
    Config(const char *keyName, const char *category, const char* displayName, T defaultValue, bool showInWeb = true, bool isPassword = false, void (*cb)(T) = nullptr)
        : BaseSetting(category, keyName, displayName, TypeTraits<T>::type, showInWeb, isPassword), value(defaultValue), defaultValue(defaultValue), originalCallback(cb)
    {
        if (hasError() && logger) {
            logger(getError());
        }
    }
    // Overload: with categoryPretty
    Config(const char *keyName, const char *category, const char* displayName, const char* categoryPretty, T defaultValue, bool showInWeb = true, bool isPassword = false, void (*cb)(T) = nullptr)
        : BaseSetting(category, keyName, displayName, categoryPretty, TypeTraits<T>::type, showInWeb, isPassword), value(defaultValue), defaultValue(defaultValue), originalCallback(cb)
    {
        if (hasError() && logger) {
            logger(getError());
        }
    }

    T get() const { return value; }

    void setCallback(std::function<void(T)> cb) {
        callback = cb;
    }

    void set(T newVal) {
        if (value != newVal) {
            value = newVal;
            modified = true;
            if (callback) callback(value);
            if (originalCallback) originalCallback(value);
        }
    }

    SettingType getType() const override { return TypeTraits<T>::type; }

    void load(Preferences &prefs) override {
        const char *key = getKey();
        if constexpr (std::is_same_v<T, int>) {
            value = prefs.getInt(key, defaultValue);
        } else if constexpr (std::is_same_v<T, bool>) {
            value = prefs.getBool(key, defaultValue);
        } else if constexpr (std::is_same_v<T, float>) {
            value = prefs.getFloat(key, defaultValue);
        } else if constexpr (std::is_same_v<T, String>) {
            value = prefs.getString(key, defaultValue);
        }
    }

    void save(Preferences &prefs) override {
        const char *key = getKey();
        if constexpr (std::is_same_v<T, int>) {
            prefs.putInt(key, value);
        } else if constexpr (std::is_same_v<T, bool>) {
            prefs.putBool(key, value);
        } else if constexpr (std::is_same_v<T, float>) {
            prefs.putFloat(key, value);
        } else if constexpr (std::is_same_v<T, String>) {
            prefs.putString(key, value);
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
        // 04.09.2025 bugfix: it will not catch the exception [   224][E][Preferences.cpp:483] getString(): nvs_get_str len fail: MQTT_Server NOT_FOUND, and overload the log until its crash
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
        server.handleClient();
    }

    void handleClientSecure() {
        server.handleClient();
        // secureServer->handleClient();
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

    void startWebServer(const String &ipStr, const String &mask, const String &ssid, const String &password) {
        log_message("🌐 Configuring static IP %s...\n", ipStr.c_str());

        IPAddress ip, gateway, subnet;
        ip.fromString(ipStr);
        gateway = ip; // Adjust if needed
        subnet.fromString(mask);

        WiFi.config(ip, gateway, subnet, IPAddress(8, 8, 8, 8));

        log_message("🔌 Connecting to WiFi SSID: %s\n", ssid.c_str());
        WiFi.mode(WIFI_STA); // Set WiFi mode to Station bugfix for unit test
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

    void startWebServer(const String &ipStr, const String &mask, const String &dnsServer, const String &ssid, const String &password) {
        log_message("🌐 Configuring static IP %s...\n", ipStr.c_str());

        IPAddress ip, gateway, subnet, dns;

        ip.fromString(ipStr);
        gateway = ip; // Adjust if needed
        subnet.fromString(mask);
        dns.fromString(dnsServer);

        WiFi.config(ip, gateway, subnet, dns);

        log_message("🔌 Connecting to WiFi SSID: %s\n", ssid.c_str());
        WiFi.mode(WIFI_STA); // Set WiFi mode to Station bugfix for unit test
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

    void defineRoutes() {
        log_message("🌐 Defining routes...");

        // Serve version at /version
        server.on("/version", HTTP_GET, []() {
            server.send(200, "text/plain", CONFIGMANAGER_VERSION);
        });

        server.on("/ota_update", HTTP_GET, [this]() {
            String html = R"(
                <!DOCTYPE html>
                <html>
                <body>
                    <h2>OTA Update</h2>
                    <form method='POST' action='/ota_update' enctype='multipart/form-data'>
                        <input type='file' keyName='firmware'>
                        <input type='submit' value='Update'>
                    </form>
                </body>
                </html>
            )";
            server.send(200, "text/html", html);
        });

        server.on("/ota_update", HTTP_POST, [this]() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", Update.hasError() ? "UPDATE FAILED" : "UPDATE SUCCESS");
            ESP.restart();
        }, [this]() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                Serial.printf("Update: %s\n", upload.filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) {
                    Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
                } else {
                    Update.printError(Serial);
                }
            }
        });

        // Reset to Defaults
        server.on("/config/reset", HTTP_POST, [this]() {
            for (auto *s : settings) s->setDefault();
            saveAll();
            log_message("🌐 All settings reset to default");
            server.send(200, "application/json", "{\"status\":\"reset\"}");
        });

        // Apply single setting
        server.on("/config/apply", HTTP_POST, [this]() {
            String category = server.arg("category");
            String key = server.arg("key");

            log_message("🌐 Apply: %s/%s\n", category.c_str(), key.c_str());

            DynamicJsonDocument doc(256);
            deserializeJson(doc, server.arg((size_t)2));

            for (auto *s : settings) {
                if (String(s->getCategory()) == category && String(s->getName()) == key) {
                    if (s->fromJSON(doc["value"])) {
                        log_message("✅ Setting applied Category: %s, Key: %s", category.c_str(), key.c_str());
                        server.send(200, "application/json", "{\"status\":\"applied\"}");
                        return;
                    }
                }
            }

            log_message("❌ Setting not found");
            server.send(404, "application/json", "{\"status\":\"not_found\"}");
        });

        server.on("/config/apply_all", HTTP_POST, [this]() {
            DynamicJsonDocument doc(1024);
            log_message("🌐 Applying all settings...");
            deserializeJson(doc, server.arg("plain"));
            if (fromJSON(doc)) {
                server.send(200, "application/json", "{\"status\":\"applied\"}");
            } else {
                server.send(400, "application/json", "{\"status\":\"invalid\"}");
            }
        });

        // Save single setting
        server.on("/config/save", HTTP_POST, [this]() {
            String category = server.arg("category");
            String key = server.arg("key");

            log_message("🌐 Save: %s/%s\n", category.c_str(), key.c_str());

            DynamicJsonDocument doc(256);
            deserializeJson(doc, server.arg((size_t)2));

            for (auto *s : settings) {
                if (String(s->getCategory()) == category && String(s->getName()) == key) {
                    if (s->fromJSON(doc["value"])) {
                        prefs.begin("config", false);
                        s->save(prefs);
                        prefs.end();
                        log_message("✅ Setting saved Category: %s, Key: %s", category.c_str(), key.c_str());
                        server.send(200, "application/json", "{\"status\":\"saved\"}");
                        return;
                    }
                }
            }

            log_message("❌ Setting not found category: %s, key: %s", category.c_str(), key.c_str());
            server.send(404, "application/json", "{\"status\":\"not_found\"}");
        });

        // Reboot
        server.on("/config/reboot", HTTP_POST, [this]() {
            server.send(200, "application/json", "{\"status\":\"rebooting\"}");
            log_message("🔄 Device rebooting...");
            delay(100);
            reboot();
        });

        // Config JSON
        server.on("/config.json", HTTP_GET, [this]() {
            server.send(200, "application/json", toJSON());
        });

        // Save all settings
        server.on("/config/save_all", HTTP_POST, [this]() {
            DynamicJsonDocument doc(1024);
            log_message("🌐 Saving all settings...");
            deserializeJson(doc, server.arg("plain"));
            if (fromJSON(doc)) {
                saveAll();
                server.send(200, "application/json", "{\"status\":\"saved\"}");
            } else {
                server.send(400, "application/json", "{\"status\":\"invalid\"}");
            }
        });

        // Root route
        server.on("/", HTTP_GET, [this]() {
            server.send_P(200, "text/html", webhtml.getWebHTML());
        });

        server.on("/", HTTP_POST, [this]() {
            log_message("🌐 Received POST request to root route");
            log_message("🌐 Body: %s", server.arg("plain"));
        });

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

            // log_message("WiFi Status: %d\n", WiFi.status());
            // log_message("Local IP: %s\n", WiFi.localIP().toString().c_str());
            // log_message("Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
            // log_message("Gateway IP: %s\n", WiFi.gatewayIP().toString().c_str());


            ArduinoOTA
                .onStart([this]() {
                    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
                    log_message("📡 OTA Update Start: %s", type.c_str());
                })
                .onEnd([this]() {
                    log_message("OTA Update Finished");
                })
                .onProgress([this](unsigned int progress, unsigned int total) {
                    log_message("OTA Progress: %u%%", (progress * 100) / total);
                })

                .onError([this](ota_error_t error) {
                    log_message("OTA Error[%u]: ", error);
                    if (error == OTA_AUTH_ERROR) log_message("Auth Failed");
                    else if (error == OTA_BEGIN_ERROR) log_message("Begin Failed");
                    else if (error == OTA_CONNECT_ERROR) log_message("Connect Failed");
                    else if (error == OTA_RECEIVE_ERROR) log_message("Receive Failed");
                    else if (error == OTA_END_ERROR) log_message("End Failed");
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
            log_message("OTA: Waiting for WiFi connection");
            // log_message("WiFi Status: %d\n", WiFi.status());
        }
    }

    void stopOTA() {
        if (_otaInitialized) {
            //TODO: No direct method to stop ArduinoOTA how to handle this?
             ArduinoOTA.end(); // Hypothetical function, does not exist in current library
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
        WebHTML webhtml;
        static LogCallback logger;

        bool _otaEnabled = false;
        bool _otaInitialized = false;
        String _otaPassword;
        String _otaHostname;

};



// extern ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr;
extern ConfigManagerClass ConfigManager;