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

static const char SETTINGS_PASSWORD[] = "";

static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

static cm::MQTTManager& mqtt = cm::MQTTManager::instance();

// Receive demo values
static float boilerTemperatureC = 0.0f;
static String boilerTimeRemaining;
static bool boilerYouCanShowerNow = false;

static float powerMeterPowerInW = 0.0f;

static float washingMachineEnergyTotal = 0.0f;
static float washingMachineEnergyYesterday = 0.0f;
static float washingMachineEnergyTotalMWh = 0.0f;


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

    mqtt.attach(ConfigManager);

    // Preferred hooks (structured callbacks)
    mqtt.onMQTTConnect([]() {
        CM_LOG("[Full-MQTT-Demo][INFO] MQTT connected");
        if (mqtt.settings().publishTopicBase.get().isEmpty()) {
            mqtt.settings().publishTopicBase.set(mqtt.settings().clientId.get());
        }
        const bool ok = mqtt.publishSystemInfoNow(true);
        if (!ok) {
            CM_LOG("[Full-MQTT-Demo][WARNING] Failed to publish System-Info (missing base topic or not connected)");
        }
    });
    mqtt.onMQTTDisconnect([]() { CM_LOG("[Full-MQTT-Demo][INFO] MQTT disconnected"); });
    mqtt.onMQTTStateChanged([](cm::MQTTManager::ConnectionState state) {
        CM_LOG("[Full-MQTT-Demo][INFO] MQTT state changed: %s", cm::MQTTManager::mqttStateToString(state));
    });
    mqtt.onNewMQTTMessage([](const cm::MQTTManager::MqttMessageView& msg) {
        String payload(reinterpret_cast<const char*>(msg.payload), msg.length);
        payload.trim();
        CM_LOG((String("[Full-MQTT-Demo][INFO] MQTT RX: ") + (msg.topic ? msg.topic : "") + " => " + payload).c_str());
    });

    // Classic callbacks (mirror PubSubClient signatures)
    // mqtt.onConnected([]() { CM_LOG("[Full-MQTT-Demo][INFO] MQTT connected (classic)"); });
    // mqtt.onDisconnected([]() { CM_LOG("[Full-MQTT-Demo][INFO] MQTT disconnected (classic)"); });
    // mqtt.onMessage([](char* topic, byte* payload, unsigned int length) { /* ... */ });

    // Receive test topics
    mqtt.addMQTTTopicReceiveFloat("boiler_temp_c", "Boiler Temperature", "BoilerSaver/TemperatureBoiler", &boilerTemperatureC, "C", 1, "none", true);
    mqtt.addMQTTTopicReceiveString("boiler_time_remaining", "Boiler Time Remaining", "BoilerSaver/TimeRemaining", &boilerTimeRemaining, "none", true);
    mqtt.addMQTTTopicReceiveBool("boiler_shower_now", "You Can Shower Now", "BoilerSaver/YouCanShowerNow", &boilerYouCanShowerNow, "none", true);
    mqtt.addMQTTTopicReceiveFloat("powermeter_power_in_w", "Power Meter Power In", "tele/powerMeter/powerMeter/SENSOR", &powerMeterPowerInW, "W", 0, "E320.Power_in", true);

    mqtt.addMQTTTopicReceiveFloat("washing_machine_energy_total", "Washing Machine Energy Total", "tele/tasmota_1DEE45/SENSOR", &washingMachineEnergyTotal, "kWh", 3, "ENERGY.Total", true);
    mqtt.addMQTTTopicReceiveFloat("washing_machine_energy_yesterday", "Washing Machine Energy Yesterday", "tele/tasmota_1DEE45/SENSOR", &washingMachineEnergyYesterday, "kWh", 3, "ENERGY.Yesterday", true);

    mqtt.addToGUI(ConfigManager, "mqtt", 2, 10);

    // GUI examples: explicitly opt-in the receive fields
    mqtt.addMQTTTopicTooGUI(ConfigManager, "boiler_temp_c", "MQTT-Received",1);
    mqtt.addMQTTTopicTooGUI(ConfigManager, "powermeter_power_in_w", "MQTT-Received",2);

    // GUI examples: other infos via runtime provider
    ConfigManager.getRuntime().addRuntimeProvider("mqtt", [](JsonObject& data) {
        data["lastTopic"] = mqtt.getLastTopic();
        data["lastPayload"] = mqtt.getLastPayload();
        data["lastMsgAgeMs"] = mqtt.getLastMessageAgeMs();
        data["washing_machine_energy_total_mwh"] = washingMachineEnergyTotalMWh;
    }, 3);

    RuntimeFieldMeta lastTopicMeta;
    lastTopicMeta.group = "mqtt";
    lastTopicMeta.key = "lastTopic";
    lastTopicMeta.label = "Last Topic";
    lastTopicMeta.order = 22;
    lastTopicMeta.card = "MQTT Other Infos";
    ConfigManager.getRuntime().addRuntimeMeta(lastTopicMeta);

    RuntimeFieldMeta lastPayloadMeta;
    lastPayloadMeta.group = "mqtt";
    lastPayloadMeta.key = "lastPayload";
    lastPayloadMeta.label = "Last Payload";
    lastPayloadMeta.isString = true;
    lastPayloadMeta.order = 21;
    lastPayloadMeta.card = "MQTT Other Infos";
    ConfigManager.getRuntime().addRuntimeMeta(lastPayloadMeta);

    RuntimeFieldMeta lastAgeMeta;
    lastAgeMeta.group = "mqtt";
    lastAgeMeta.key = "lastMsgAgeMs";
    lastAgeMeta.label = "Last Message Age";
    lastAgeMeta.unit = "ms";
    lastAgeMeta.order = 20;
    lastAgeMeta.card = "MQTT Other Infos";
    ConfigManager.getRuntime().addRuntimeMeta(lastAgeMeta);

    RuntimeFieldMeta mwhMeta;
    mwhMeta.group = "mqtt";
    mwhMeta.key = "washing_machine_energy_total_mwh";
    mwhMeta.label = "Washing Machine Energy Total";
    mwhMeta.unit = "MWh";
    mwhMeta.precision = 2;
    mwhMeta.order = 4;
    mwhMeta.card = "MQTT-Received";
    ConfigManager.getRuntime().addRuntimeMeta(mwhMeta);

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

    ConfigManager.enableWebSocketPush();
    ConfigManager.setWebSocketInterval(1000);
    ConfigManager.setPushOnConnect(true);

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

    mqtt.loop();

    washingMachineEnergyTotalMWh = washingMachineEnergyTotal / 1000.0f;
    {
        const String base = mqtt.getMqttBaseTopic();
        if (!base.isEmpty()) {
            char payload[16];
            snprintf(payload, sizeof(payload), "%.2f", static_cast<double>(washingMachineEnergyTotalMWh));
            const String topic = base + "/washing_machine_energy_total_mwh";
            mqtt.publishExtraTopic("washing_machine_energy_total_mwh", topic.c_str(), payload, true);
        }
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
