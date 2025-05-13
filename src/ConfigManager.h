#pragma once


//⚠️ Atention ⚠️ settings will not be stored if its lenht >14!, because the max length for prefs is 15 chars...
// The settings wil be stored in the format: <category>_<key>

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include <functional>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WebServer.h>

#include "html_content.h"

// #define ENABLE_LOGGING

#ifdef ENABLE_LOGGING
#define log(fmt, ...)                                     \
  do                                                      \
  {                                                       \
    char buffer[255];                                     \
    snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__); \
    Serial.println(buffer);                               \
  } while (0)
#else
#define log(...) \
  do             \
  {              \
  } while (0) // do nothing if logging is disabled
#endif

extern WebServer server;
#define sl Serial

enum class SettingType { BOOL, INT, FLOAT, STRING, PASSWORD };

template <typename T> struct TypeTraits {};
template <> struct TypeTraits<bool> { static constexpr SettingType type = SettingType::BOOL; };
template <> struct TypeTraits<int> { static constexpr SettingType type = SettingType::INT; };
template <> struct TypeTraits<float> { static constexpr SettingType type = SettingType::FLOAT; };
template <> struct TypeTraits<String> { static constexpr SettingType type = SettingType::STRING; };

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

    const char *getKey() const {
        static char key[64];
        snprintf(key, sizeof(key), "%s_%s", category, name);
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

        // Verhindere, dass "***" als echtes Passwort gespeichert wird
        if (isSecret() && val.is<const char*>() && strcmp(val.as<const char*>(), "***") == 0) {
            // Maskiertes Passwort – nichts ändern
            return false;
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
private:
    Preferences prefs;
    std::vector<BaseSetting *> settings;
    WebHTML webhtml;

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

    bool getWiFiStatus()
    {
        return WiFi.status() == WL_CONNECTED;
    }

    void reboot()
    {
        log("🔄 Rebooting...");
        delay(1000);
        ESP.restart();
    }

    void handleClient()
    {
        server.handleClient();
    }

    void handleClientSecure()
    {
        server.handleClient();
        // secureServer->handleClient();
    }

    void reconnectWifi()
    {
        log("🔄 Reconnecting to WiFi...");
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


    void startAccessPoint()
    {
        startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", "config1234");
    }

    void startAccessPoint(const String &pwd)
    {
        startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", pwd);
    }

    void startAccessPoint(const String &APName,const String &pwd)
    {
        startAccessPoint("192.168.4.1", "255.255.255.0", APName, pwd);
    }

    void startAccessPoint(const String &ipStr, const String &mask, const String &APName, const String &pwd)
    {
        log("🌐 Konfiguriere AP %s ...\n", APName.c_str());
        log("🌐 Konfiguriere statische IP %s ...\n", ipStr.c_str());
        log("🌐 Konfiguriere Subnetzmaske %s ...\n", mask.c_str());
        log("🌐 Konfiguriere Passwort %s ...\n", pwd.c_str());
        log("🌐 Konfiguriere Gateway %s ...\n", ipStr.c_str());

        IPAddress localIP, gateway, subnet;
        localIP.fromString(ipStr);
        gateway = localIP;
        subnet.fromString(mask);

        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(localIP, gateway, subnet);

        if (strcmp(pwd.c_str(), "") == 0)
        {
            log("🌐 AP ohne Passwort");
            WiFi.softAP(APName);
        }
        else
        {
            log("🌐 AP mit Passwort: %s", pwd.c_str());
             WiFi.softAP(APName, pwd);
        }
        log("🌐 AP gestartet unter: %s", WiFi.softAPIP().toString().c_str());
        defineRoutes();
        server.begin();
    }

    void startWebServer(const String &ipStr, const String &mask, const String &ssid, const String &password)
    {
        log("🌐 Konfiguriere statische IP %s ...\n", ipStr.c_str());

        IPAddress ip, gateway, subnet;
        ip.fromString(ipStr);
        gateway = ip; // ggf. anpassen
        subnet.fromString(mask);

        WiFi.config(ip, gateway, subnet);

        log("🔌 Verbinde mit WLAN SSID: %s\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());

        unsigned long startAttemptTime = millis();
        const unsigned long timeout = 10000;

        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout)
        {
            delay(250);
            log(".");
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            log("\n✅ WLAN verbunden!");
            log("🌐 IP-Adresse: %s", WiFi.localIP().toString().c_str());

            log("🌐 Routen definieren...");
            defineRoutes();

            log("🌐 Starte Webserver...");
            server.begin();

            log("🖥️  Webserver läuft unter:%s",WiFi.localIP().toString().c_str());
        }
        else
        {
            log("\n❌ Verbindung zum WLAN fehlgeschlagen!");
        }
    }

    void startWebServer(const String &ssid, const String &password)
    {
        log("🌐 DHCP-Modus aktiv – Verbinde mit WLAN...");
        WiFi.begin(ssid.c_str(), password.c_str());

        unsigned long startAttemptTime = millis();
        const unsigned long timeout = 10000;

        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout)
        {
            delay(250);
            log(".");
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            log("\n✅ WLAN verbunden mit DHCP!");
            log("🌐 IP-Adresse: %s", WiFi.localIP().toString().c_str());

            defineRoutes();
            server.begin();
        }
        else
        {
            log("\n❌ Verbindung mit DHCP fehlgeschlagen!");
        }
    }

    void defineRoutes() {
        log("🌐 Definiere Routen...");

        // Reset to Defaults
        server.on("/config/reset", HTTP_POST, [this]() {
            for (auto *s : settings) s->setDefault();
            saveAll();
            log("🌐 Alle Einstellungen auf Standard zurückgesetzt");
            server.send(200, "application/json", "{\"status\":\"reset\"}");
        });


        // Apply single setting
        server.on("/config/apply", HTTP_POST, [this]() {
            String category = server.arg("category");
            String key = server.arg("key");

            log("🌐 Apply: %s/%s\n", category.c_str(), key.c_str());

            DynamicJsonDocument doc(256);
            deserializeJson(doc, server.arg((size_t)2));

            log("Arg(0):[%s]",server.arg((size_t)0));
            log("Arg(1):[%s]",server.arg((size_t)1));
            log("Arg(2):[%s]",server.arg((size_t)2));
            log("");

            for (auto *s : settings) {
                if (String(s->getCategory()) == category && String(s->getName()) == key) {
                    if (s->fromJSON(doc["value"])) {
                        log("✅ Setting applied");
                        server.send(200, "application/json", "{\"status\":\"applied\"}");
                        return;
                    }
                }
            }

            log("❌ Setting not found");
            server.send(404, "application/json", "{\"status\":\"not_found\"}");
        });

        // Save single setting
        server.on("/config/save", HTTP_POST, [this]() {
            String category = server.arg("category");
            String key = server.arg("key");

            log("🌐 save: %s/%s\n", category.c_str(), key.c_str());

            DynamicJsonDocument doc(256);
            deserializeJson(doc, server.arg((size_t)2));

            for (auto *s : settings) {
                if (String(s->getCategory()) == category && String(s->getName()) == key) {
                    if (s->fromJSON(doc["value"])) {
                        prefs.begin("config", false);
                        s->save(prefs);
                        prefs.end();
                        log("✅ Setting saved");
                        server.send(200, "application/json", "{\"status\":\"saved\"}");
                        return;
                    }
                }
            }

            log("❌ Setting not found");
            server.send(404, "application/json", "{\"status\":\"not_found\"}");
        });

        // Reboot
        server.on("/reboot", HTTP_POST, [this]() {
            server.send(200, "application/json", "{\"status\":\"rebooting\"}");
            log("🔄 Neustart des Geräts...");
            delay(100);
            reboot();
        });

        // Config JSON
        server.on("/config.json", HTTP_GET, [this]() {
            server.send(200, "application/json", toJSON());
        });

        // Save all settings
        server.on("/save", HTTP_POST, [this]() {
            DynamicJsonDocument doc(1024);
            log("🌐 Speichern aller Einstellungen...");
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
            log("🌐 POST-Anfrage an die Root-Route erhalten");
            log("🌐 Body: %s", server.arg("plain"));
        });

        log("🌐 Routen erfolgreich definiert");
    }

};

extern ConfigManagerClass ConfigManager;
