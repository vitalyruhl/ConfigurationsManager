#include <Arduino.h>

#include <Ticker.h>
#include <WiFi.h>

#include "ConfigManager.h"

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"

// Optional MQTT module (requires PubSubClient in the consuming project)
#include "mqtt/MQTTManager.h"

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-Full-MQTT-Demo"

extern ConfigManagerClass ConfigManager;
static inline ConfigManagerRuntime &CRM() { return ConfigManager.getRuntime(); }

static const char SETTINGS_PASSWORD[] = "cm";

static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

static cm::MQTTManager mqtt;

// MQTT Settings (example-local; module does not auto-register settings)
static Config<bool> mqttEnable(ConfigOptions<bool>{
    .key = "mqttEn",
    .name = "MQTT: Enable",
    .category = "MQTT",
    .defaultValue = false});

static Config<String> mqttServer(ConfigOptions<String>{
    .key = "mqttHost",
    .name = "MQTT: Server",
    .category = "MQTT",
    .defaultValue = String(""),
    .isPassword = false});

static Config<int> mqttPort(ConfigOptions<int>{
    .key = "mqttPort",
    .name = "MQTT: Port",
    .category = "MQTT",
    .defaultValue = 1883});

static Config<String> mqttUsername(ConfigOptions<String>{
    .key = "mqttUser",
    .name = "MQTT: Username",
    .category = "MQTT",
    .defaultValue = String(""),
    .isPassword = false});

static Config<String> mqttPassword(ConfigOptions<String>{
    .key = "mqttPass",
    .name = "MQTT: Password",
    .category = "MQTT",
    .defaultValue = String(""),
    .isPassword = true});

static Config<String> mqttSubscribeTopic(ConfigOptions<String>{
    .key = "mqttSub",
    .name = "MQTT: Subscribe Topic",
    .category = "MQTT",
    .defaultValue = String("cm/demo/#")});

static Config<String> mqttPublishTopic(ConfigOptions<String>{
    .key = "mqttPub",
    .name = "MQTT: Publish Topic",
    .category = "MQTT",
    .defaultValue = String("cm/demo/status")});

// Runtime state for GUI
static String mqttLastTopic;
static String mqttLastPayload;
static unsigned long mqttLastMessageMs = 0;

static const char *mqttStateToString(cm::MQTTManager::ConnectionState state)
{
    switch (state) {
    case cm::MQTTManager::ConnectionState::Disconnected:
        return "disconnected";
    case cm::MQTTManager::ConnectionState::Connecting:
        return "connecting";
    case cm::MQTTManager::ConnectionState::Connected:
        return "connected";
    case cm::MQTTManager::ConnectionState::Failed:
        return "failed";
    }
    return "unknown";
}

static void configureMqttFromSettings()
{
    if (!mqttEnable.get()) {
        mqtt.disconnect();
        return;
    }

    mqtt.setServer(mqttServer.get().c_str(), static_cast<uint16_t>(mqttPort.get()));

    if (mqttUsername.get().length() > 0) {
        mqtt.setCredentials(mqttUsername.get().c_str(), mqttPassword.get().c_str());
    } else {
        mqtt.setCredentials("", "");
    }

    mqtt.setKeepAlive(60);
    mqtt.setMaxRetries(10);
    mqtt.setRetryInterval(5000);

    mqtt.onConnected([]() {
        const String sub = mqttSubscribeTopic.get();
        if (sub.length() > 0) {
            mqtt.subscribe(sub.c_str());
        }

        const String pub = mqttPublishTopic.get();
        if (pub.length() > 0) {
            mqtt.publish(pub.c_str(), "online", true);
        }

        Serial.println("[MQTT] connected");
    });

    mqtt.onDisconnected([]() {
        Serial.println("[MQTT] disconnected");
    });

    mqtt.onMessage([](char *topic, byte *payload, unsigned int length) {
        mqttLastTopic = String(topic);
        mqttLastPayload = String(reinterpret_cast<const char *>(payload), length);
        mqttLastMessageMs = millis();
    });

    mqtt.begin();
}

static void setupGUI()
{
    CRM().addRuntimeProvider("mqtt", [](JsonObject &data) {
        data["enabled"] = mqttEnable.get();
        data["wifi"] = WiFi.isConnected();
        data["state"] = mqttStateToString(mqtt.getState());
        data["reconnects"] = mqtt.getReconnectCount();
        data["retry"] = mqtt.getCurrentRetry();
        data["uptimeMs"] = mqtt.getUptime();
        data["lastTopic"] = mqttLastTopic;
        data["lastPayload"] = mqttLastPayload;
        data["lastMsgAgeMs"] = mqttLastMessageMs > 0 ? (millis() - mqttLastMessageMs) : 0;
    }, 2);

    RuntimeFieldMeta enabledMeta;
    enabledMeta.group = "mqtt";
    enabledMeta.key = "enabled";
    enabledMeta.label = "MQTT Enabled";
    enabledMeta.order = 10;
    enabledMeta.isBool = true;
    CRM().addRuntimeMeta(enabledMeta);

    RuntimeFieldMeta wifiMeta;
    wifiMeta.group = "mqtt";
    wifiMeta.key = "wifi";
    wifiMeta.label = "WiFi Connected";
    wifiMeta.order = 11;
    wifiMeta.isBool = true;
    CRM().addRuntimeMeta(wifiMeta);

    RuntimeFieldMeta stateMeta;
    stateMeta.group = "mqtt";
    stateMeta.key = "state";
    stateMeta.label = "State";
    stateMeta.order = 12;
    CRM().addRuntimeMeta(stateMeta);

    RuntimeFieldMeta reconnectsMeta;
    reconnectsMeta.group = "mqtt";
    reconnectsMeta.key = "reconnects";
    reconnectsMeta.label = "Reconnect Count";
    reconnectsMeta.order = 13;
    CRM().addRuntimeMeta(reconnectsMeta);

    RuntimeFieldMeta retryMeta;
    retryMeta.group = "mqtt";
    retryMeta.key = "retry";
    retryMeta.label = "Current Retry";
    retryMeta.order = 14;
    CRM().addRuntimeMeta(retryMeta);

    RuntimeFieldMeta uptimeMeta;
    uptimeMeta.group = "mqtt";
    uptimeMeta.key = "uptimeMs";
    uptimeMeta.label = "MQTT Uptime";
    uptimeMeta.order = 15;
    uptimeMeta.unit = "ms";
    CRM().addRuntimeMeta(uptimeMeta);

    RuntimeFieldMeta lastTopicMeta;
    lastTopicMeta.group = "mqtt";
    lastTopicMeta.key = "lastTopic";
    lastTopicMeta.label = "Last Topic";
    lastTopicMeta.order = 20;
    CRM().addRuntimeMeta(lastTopicMeta);

    RuntimeFieldMeta lastPayloadMeta;
    lastPayloadMeta.group = "mqtt";
    lastPayloadMeta.key = "lastPayload";
    lastPayloadMeta.label = "Last Payload";
    lastPayloadMeta.order = 21;
    CRM().addRuntimeMeta(lastPayloadMeta);

    RuntimeFieldMeta lastAgeMeta;
    lastAgeMeta.group = "mqtt";
    lastAgeMeta.key = "lastMsgAgeMs";
    lastAgeMeta.label = "Last Message Age";
    lastAgeMeta.order = 22;
    lastAgeMeta.unit = "ms";
    CRM().addRuntimeMeta(lastAgeMeta);

    ConfigManager.defineRuntimeButton("mqtt", "pubPing", "Publish Ping", []() {
        if (!mqttEnable.get()) {
            Serial.println("[MQTT] publish ping skipped: MQTT disabled");
            return;
        }
        const String pub = mqttPublishTopic.get();
        if (pub.length() == 0) {
            Serial.println("[MQTT] publish ping skipped: publish topic empty");
            return;
        }

        const String msg = String("ping ") + String(millis());
        const bool ok = mqtt.publish(pub.c_str(), msg.c_str(), false);
        Serial.printf("[MQTT] publish ping: %s\n", ok ? "ok" : "failed");
    }, "", 23);
}

void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

void setup()
{
    Serial.begin(115200);

    ConfigManagerClass::setLogger([](const char *msg) {
        Serial.print("[ConfigManager] ");
        Serial.println(msg);
    });

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(VERSION);
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
    ConfigManager.enableBuiltinSystemProvider();

    coreSettings.attachWiFi(ConfigManager);
    coreSettings.attachSystem(ConfigManager);
    coreSettings.attachNtp(ConfigManager);

    ConfigManager.addSetting(&mqttEnable);
    ConfigManager.addSetting(&mqttServer);
    ConfigManager.addSetting(&mqttPort);
    ConfigManager.addSetting(&mqttUsername);
    ConfigManager.addSetting(&mqttPassword);
    ConfigManager.addSetting(&mqttSubscribeTopic);
    ConfigManager.addSetting(&mqttPublishTopic);

    ConfigManager.checkSettingsForErrors();
    ConfigManager.loadAll();

    systemSettings.allowOTA.setCallback([](bool enabled) {
        Serial.printf("[MAIN] OTA setting changed to: %s\n", enabled ? "enabled" : "disabled");
        ConfigManager.getOTAManager().enable(enabled);
    });
    ConfigManager.getOTAManager().enable(systemSettings.allowOTA.get());

    // Settings-driven WiFi startup (DHCP/static/AP fallback).
    ConfigManager.startWebServer();
    ConfigManager.getWiFiManager().setAutoRebootTimeout((unsigned long)systemSettings.wifiRebootTimeoutMin.get());

    setupGUI();

    ConfigManager.enableWebSocketPush();
    ConfigManager.setWebSocketInterval(1000);
    ConfigManager.setPushOnConnect(true);

    configureMqttFromSettings();

    // Apply MQTT config when any MQTT setting changes.
    mqttEnable.setCallback([](bool) { configureMqttFromSettings(); });
    mqttServer.setCallback([](String) { configureMqttFromSettings(); });
    mqttPort.setCallback([](int) { configureMqttFromSettings(); });
    mqttUsername.setCallback([](String) { configureMqttFromSettings(); });
    mqttPassword.setCallback([](String) { configureMqttFromSettings(); });
    mqttSubscribeTopic.setCallback([](String) { configureMqttFromSettings(); });
    mqttPublishTopic.setCallback([](String) { configureMqttFromSettings(); });

    Serial.println("[MAIN] Setup completed successfully. Starting main loop...");
}

void loop()
{
    ConfigManager.updateLoopTiming();
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();
    ConfigManager.handleWebsocketPush();
    ConfigManager.handleOTA();
    ConfigManager.handleRuntimeAlarms();

    static unsigned long lastAlarmEval = 0;
    if (millis() - lastAlarmEval > 1500) {
        lastAlarmEval = millis();
        CRM().updateAlarms();
    }

    if (mqttEnable.get()) {
        mqtt.loop();
    }

    delay(10);
}

// Global WiFi event hooks used by ConfigManager.
void onWiFiConnected()
{
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    Serial.printf("[INFO] Station Mode: http://%s\n", WiFi.localIP().toString().c_str());
}

void onWiFiDisconnected()
{
    wifiServices.onDisconnected();
}

void onWiFiAPMode()
{
    wifiServices.onAPMode();
    Serial.printf("[INFO] AP Mode: http://%s\n", WiFi.softAPIP().toString().c_str());
}
