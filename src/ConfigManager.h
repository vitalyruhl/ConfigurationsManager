#pragma once

//⚠️ Warning ⚠️ Settings will not be stored if their key length >14! The max key length for preferences is 15 chars.
// Settings are stored in the format: <category>_<key>

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include <functional>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <exception>

#include "html_content.h"

//TODO Global
//ToDo: add optional parameter to set the keyname and Kategoryname for the webUI

extern WebServer server;
class ConfigManagerClass; // Forward declaration

enum class SettingType { BOOL, INT, FLOAT, STRING, PASSWORD };

template <typename T> struct TypeTraits {};
template <> struct TypeTraits<bool> { static constexpr SettingType type = SettingType::BOOL; };
template <> struct TypeTraits<int> { static constexpr SettingType type = SettingType::INT; };
template <> struct TypeTraits<float> { static constexpr SettingType type = SettingType::FLOAT; };
template <> struct TypeTraits<String> { static constexpr SettingType type = SettingType::STRING; };


class ConfigException : public std::exception {
protected:
    String msg;
public:
    explicit ConfigException(const String& message) : msg(message) {}
    const char* what() const noexcept override { return msg.c_str(); }
};

class NonFatalException : public std::exception {
protected:
    std::string message;
public:
    NonFatalException(const std::string& msg) : message(msg) {
        //todo: add a logger, or something like that
        String logMsg = "[WARN] " + String(msg.c_str());
        // if (!Serial) {
        //     Serial.begin(115200);
        //     while (!Serial); // Warte auf Verbindung
        // }
        // Serial.println(logMsg);
        // Serial.flush();
    }
    const char* what() const noexcept override {
        return message.c_str();
    }
};

class KeyTooLongException : public std::exception {
    String msg;
public:
    KeyTooLongException(const char* cat, const char* name) {
        msg = "Category '" + String(cat) + "' exceeds 13 chars (Name: '" + name + "')\n";
        msg += " ESP32 Preferences only allows 15 chars in total (Category + _ + Name)\n";
        msg += " Category: 'network_config' (14 Chars) → Error (max 13 allowed)\n";
        msg += " Category: 'wifi' + Name: 'very_long_password_setting' → Name cuts to 'very_lo' (15 - 4 - 1 = 10 chars) → its ok\n";


    }
    const char* what() const noexcept override {
        return msg.c_str();
    }
};

class KeyTruncatedWarning : public NonFatalException {
public:
    KeyTruncatedWarning(const std::string& original, const std::string& trimmed)
        : NonFatalException("Key truncated: '" + original + "' → '" + trimmed + "'") {}
};

class BaseSetting {
    protected:
    const char *name;
    const char *category;
    bool showInWeb;
    bool isPassword;
    bool modified = false;

    public:
    BaseSetting(const char *name, const char *category, bool showInWeb, bool isPassword)
    : name(name), category(category), showInWeb(showInWeb), isPassword(isPassword) {}

    virtual ~BaseSetting() = default;
    virtual SettingType getType() const = 0;
    virtual void load(Preferences &prefs) = 0;
    virtual void save(Preferences &prefs) = 0;
    virtual void setDefault() = 0;
    virtual void toJSON(JsonObject &obj) const = 0;
    virtual bool fromJSON(const JsonVariant &value) = 0;

    const char* getKey() const {
        static char key[64]; // 15 chars + Null-Terminator
        const int MAX_CATEGORY_LENGTH = 13;
        const int MAX_TOTAL_LENGTH = 15;

        if (strlen(category) > MAX_CATEGORY_LENGTH) {
            throw KeyTooLongException(category, name);
        }

        int maxNameLength = MAX_TOTAL_LENGTH - strlen(category) - 1;

        char nameTrimmed[maxNameLength + 1];
        strncpy(nameTrimmed, name, maxNameLength);
        nameTrimmed[maxNameLength] = '\0';

        if (strlen(name) > maxNameLength) {
            throw KeyTruncatedWarning(name, nameTrimmed);
        }

        snprintf(key, sizeof(key), "%s_%s", category, nameTrimmed);
        return key;
    }

    bool isSecret() const { return isPassword; }
    const char *getCategory() const { return category; }
    const char *getName() const { return name; }
    bool shouldShowInWeb() const { return showInWeb; }
    bool needsSave() const { return modified; }

};

template <typename T> class Config : public BaseSetting {
    private:
    T value;
    T defaultValue;
    void (*callback)(T);

    public:
    Config(const char *name, const char *category, T defaultValue, bool showInWeb = true, bool isPassword = false, void (*cb)(T) = nullptr)
            : BaseSetting(name, category, showInWeb, isPassword), value(defaultValue), defaultValue(defaultValue), callback(cb) {}

    T get() const { return value; }

    void set(T newVal) {
        if (value != newVal) {
            value = newVal;
            modified = true;
            if (callback) callback(value);
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

    void toJSON(JsonObject &obj) const override
        {
            if (isSecret())
            {
                obj[name] = "***";
            }
            else
            {
                obj[name] = value;
            }
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

    BaseSetting *findSetting(const String &category, const String &key)
        {
            for (auto *s : settings)
            {
                if (String(s->getCategory()) == category && String(s->getName()) == key)
                {
                    return s;
                }
            }
            return nullptr;
        }

    void addSetting(BaseSetting *s) { settings.push_back(s); }

    void loadAll() {
        prefs.begin("config", true);
        for (auto *s : settings) s->load(prefs);
        prefs.end();
    }

    void saveAll() {
        prefs.begin("config", false);
        for (auto *s : settings) if (s->needsSave()) s->save(prefs);
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
        for (auto *s : settings) {
            const char *category = s->getCategory();
            const char *name = s->getName();
            if (!root[category].is<JsonObject>()) root.createNestedObject(category);
            JsonObject cat = root[category];
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

        WiFi.config(ip, gateway, subnet);

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

private:
    Preferences prefs;
    std::vector<BaseSetting *> settings;
    WebHTML webhtml;
    static LogCallback logger;

};

ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr;
extern ConfigManagerClass ConfigManager;