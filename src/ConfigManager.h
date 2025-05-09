#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include <functional>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"

extern WebServer server;

enum class SettingType
{
    BOOL,
    INT,
    FLOAT,
    STRING,
    PASSWORD
};

// Type-Traits für C++11
template <typename T>
struct TypeTraits
{
};
template <>
struct TypeTraits<bool>
{
    static const SettingType type = SettingType::BOOL;
};
template <>
struct TypeTraits<int>
{
    static const SettingType type = SettingType::INT;
};
template <>
struct TypeTraits<float>
{
    static const SettingType type = SettingType::FLOAT;
};
template <>
struct TypeTraits<String>
{
    static const SettingType type = SettingType::STRING;
};

class BaseSetting
{
protected:
    const char *name;
    const char *category;
    bool showInWeb;
    bool isPassword;
    bool modified = false;

public:
    BaseSetting(const char *name, const char *category, bool showInWeb, bool isPassword)
        : name(name), category(category), showInWeb(showInWeb), isPassword(isPassword) {}

    virtual SettingType getType() const = 0;
    virtual void load(Preferences &prefs) = 0;
    virtual void save(Preferences &prefs) = 0;
    virtual void setDefault() = 0;
    virtual void toJSON(JsonObject &obj) const = 0;
    virtual bool fromJSON(const JsonVariant &value) = 0;

    const char *getKey() const
    {
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

template <typename T>
class Config : public BaseSetting
{
private:
    T value;
    T defaultValue;
    void (*callback)(T);

public:
    Config(const char *name, const char *category, T defaultValue,
           bool showInWeb = true, bool isPassword = false,
           void (*cb)(T) = nullptr)
        : BaseSetting(name, category, showInWeb, isPassword),
          value(defaultValue), defaultValue(defaultValue), callback(cb) {}

    T get() const { return value; }

    void set(T newVal)
    {
        if (value != newVal)
        {
            value = newVal;
            modified = true;
            if (callback)
                callback(value);
        }
    }

    SettingType getType() const override
    {
        return TypeTraits<T>::type;
    }

    void load(Preferences &prefs) override
    {
        const char *key = getKey();
        if (std::is_same<T, int>::value)
        {
            value = prefs.getInt(key, defaultValue);
        }
        else if (std::is_same<T, bool>::value)
        {
            value = prefs.getBool(key, defaultValue);
        }
        else if (std::is_same<T, float>::value)
        {
            value = prefs.getFloat(key, defaultValue);
        }
        else if (std::is_same<T, String>::value)
        {
            value = prefs.getString(key, defaultValue);
        }
    }

    void save(Preferences &prefs) override
    {
        const char *key = getKey();
        if (std::is_same<T, int>::value)
        {
            prefs.putInt(key, value);
        }
        else if (std::is_same<T, bool>::value)
        {
            prefs.putBool(key, value);
        }
        else if (std::is_same<T, float>::value)
        {
            prefs.putFloat(key, value);
        }
        else if (std::is_same<T, String>::value)
        {
            prefs.putString(key, value);
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
        if (isSecret())
        {
            obj[name] = "***";
        }
        else
        {
            obj[name] = value;
        }
    }

    bool fromJSON(const JsonVariant &val) override
    {
        if (val.isNull())
            return false;

        if (std::is_same<T, int>::value)
        {
            set(val.as<int>());
        }
        else if (std::is_same<T, bool>::value)
        {
            set(val.as<bool>());
        }
        else if (std::is_same<T, float>::value)
        {
            set(val.as<float>());
        }
        else if (std::is_same<T, String>::value)
        {
            set(val.as<String>());
        }
        return true;
    }
};

class ConfigManagerClass
{
private:
    Preferences prefs;
    std::vector<BaseSetting *> settings;

public:
    void addSetting(BaseSetting *s) { settings.push_back(s); }

    void loadAll()
    {
        prefs.begin("config", true);
        for (auto *s : settings)
            s->load(prefs);
        prefs.end();
    }

    void saveAll()
    {
        prefs.begin("config", false);
        for (auto *s : settings)
            if (s->needsSave())
                s->save(prefs);
        prefs.end();
    }

    String toJSON(bool includeSecrets = false)
    {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();

        for (auto *s : settings)
        {
            const char *category = s->getCategory();
            const char *name = s->getName();

            if (!root[category].is<JsonObject>())
            {
                root.createNestedObject(category);
            }

            JsonObject cat = root[category];
            if (s->isSecret() && !includeSecrets)
            {
                cat[name] = "***";
            }
            else
            {
                s->toJSON(cat);
            }
        }

        String output;
        serializeJsonPretty(doc, output);
        return output;
    }

    bool fromJSON(JsonDocument &doc)
    {
        bool changed = false;
        for (auto *s : settings)
        {
            JsonVariant val = doc[s->getCategory()][s->getName()];
            changed |= s->fromJSON(val);
        }
        return changed;
    }

    void startAccessPoint()
    {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32_Config", "config1234");

        server.on("/config.json", HTTP_GET, [this]()
                  { server.send(200, "application/json", this->toJSON()); });

        server.on("/save", HTTP_POST, [this]()
                  {
            JsonDocument doc;
            deserializeJson(doc, server.arg("plain"));

            if (this->fromJSON(doc)) {
                this->saveAll();
                server.send(200, "application/json", "{\"status\":\"saved\"}");
            } else {
                server.send(400, "application/json", "{\"status\":\"invalid\"}");
            } });

        server.on("/", HTTP_GET, []()
                  {
            File file = SPIFFS.open("/index.html", "r");
            server.streamFile(file, "text/html");
            file.close(); });

        server.begin();
    }
};

extern ConfigManagerClass ConfigManager;