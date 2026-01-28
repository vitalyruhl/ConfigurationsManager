#include <Arduino.h>

#include <Ticker.h>
#include <WiFi.h>

#include "ConfigManager.h"

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"

// Optional logging module
#include "logging/LoggingManager.h"

// Optional MQTT module (requires PubSubClient in the consuming project)
#define CM_MQTT_NO_DEFAULT_HOOKS
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
static cm::LoggingManager& logManager = cm::LoggingManager::instance();

static constexpr int BUTTON_PIN = 33;
static const char BUTTON_TOPIC[] = "test_topic_Bool_send";
static const char BUTTON_ID[] = "test_topic_bool_send";
static const char TEST_PUBLISH_TOPIC[] = "test_topic_publish_immediately";
static const char TASMOTA_ERRORS_FILTER[] = "/main/error";

// Receive demo values
static float boilerTemperatureC = 0.0f;
static String boilerTimeRemaining;
static bool boilerYouCanShowerNow = false;

static float powerMeterPowerInW = 0.0f;

static float washingMachineEnergyTotal = 0.0f;
static float washingMachineEnergyYesterday = 0.0f;
static float washingMachineEnergyTotalMWh = 0.0f;
static int solarLimiterSetValueW = 0;
static String tasmotaLastError;


void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
void setupMqtt();

namespace cm {
    void onMQTTConnected()
    {
        CM_LOG("[Full-MQTT-Demo][INFO] MQTT connected");
        if (mqtt.settings().publishTopicBase.get().isEmpty()) {
            mqtt.settings().publishTopicBase.set(mqtt.settings().clientId.get());
        }
        const String base = mqtt.getMqttBaseTopic();
        if (!base.isEmpty()) {
            const String statusTopic = base + "/System-Info/status";
            mqtt.publishExtraTopicImmediately("mqtt_status_aus_Main_Callback", statusTopic.c_str(), "online", true);
        }
        const bool ok = mqtt.publishSystemInfoNow(true);
        if (!ok) {
            CM_LOG("[Full-MQTT-Demo][WARNING] Failed to publish System-Info (missing base topic or not connected)");
        }
    }

    void onMQTTDisconnected()
    {
        CM_LOG("[Full-MQTT-Demo][INFO] MQTT disconnected");
    }

    void onMQTTStateChanged(int state)
    {
        auto mqttState = static_cast<MQTTManager::ConnectionState>(state);
        CM_LOG("[Full-MQTT-Demo][INFO] MQTT state changed: %s", MQTTManager::mqttStateToString(mqttState));
    }

    void onNewMQTTMessage(const char* topic, const char* payload, unsigned int length)
    {
        if (!topic || !payload || length == 0) {
            return;
        }
        String payloadString(payload, length);
        payloadString.trim();
        CM_LOG((String("[Full-MQTT-Demo][INFO] MQTT RX: ") + topic + " => " + payloadString).c_str());

        if (String(topic).endsWith(TASMOTA_ERRORS_FILTER)) {
            tasmotaLastError = String(topic) + " => " + payloadString;
            CM_LOG((String("[TASMOTA][ERROR] ") + tasmotaLastError).c_str());
        }
    }
} // namespace cm

void setup()
{
    Serial.begin(115200);

    logManager.addOutput(std::make_unique<cm::LoggingManager::SerialOutput>(Serial));
    logManager.setGlobalLevel(cm::LoggingManager::Level::Info);
    logManager.attachToConfigManager(cm::LoggingManager::Level::Info, "CM");

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(VERSION);
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
    ConfigManager.enableBuiltinSystemProvider();

    coreSettings.attachWiFi(ConfigManager);
    coreSettings.attachSystem(ConfigManager);
    coreSettings.attachNtp(ConfigManager);

    setupMqtt();

    ConfigManager.checkSettingsForErrors();
    ConfigManager.loadAll();

    systemSettings.allowOTA.setCallback([](bool enabled) {
        CM_LOG("[Full-MQTT-Demo][INFO] OTA setting changed to: %s", enabled ? "enabled" : "disabled");
        ConfigManager.getOTAManager().enable(enabled);
    });
    ConfigManager.getOTAManager().enable(systemSettings.allowOTA.get());

    pinMode(BUTTON_PIN, INPUT_PULLDOWN);

    // Settings-driven WiFi startup (DHCP/static/AP fallback).
    ConfigManager.startWebServer();
    ConfigManager.getWiFiManager().setAutoRebootTimeout((unsigned long)systemSettings.wifiRebootTimeoutMin.get());

    ConfigManager.enableWebSocketPush();
    ConfigManager.setWebSocketInterval(1000);
    ConfigManager.setPushOnConnect(true);

    mqtt.clearRetain("test_topic_publish_immediately");
    mqtt.publishAllNow();

    mqtt.publishExtraTopicImmediately("test_topic_publish_immediately", TEST_PUBLISH_TOPIC, "1", false);
    mqtt.publishTopicImmediately("solar_limiter_set_value_w");
    CM_LOG("[Full-MQTT-Demo][INFO] Setup completed successfully. Starting main loop...");
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
    logManager.loop();

    static unsigned long lastLoopLogMs = 0;
    const unsigned long now = millis();
    if (now - lastLoopLogMs >= 10000) {
        lastLoopLogMs = now;
        CM_LOG("[Full-MQTT-Demo][INFO] Loop: wifi=%s mqtt=%s base=%s lastTopic=%s",
               WiFi.isConnected() ? "connected" : "disconnected",
               mqtt.isConnected() ? "connected" : "disconnected",
               mqtt.getMqttBaseTopic().c_str(),
               mqtt.getLastTopic().c_str());
    }

    static bool buttonStateInitialized = false;
    static bool lastButtonState = false;
    const bool buttonState = (digitalRead(BUTTON_PIN) == HIGH);
    if (!buttonStateInitialized || buttonState != lastButtonState) {
        lastButtonState = buttonState;
        buttonStateInitialized = true;
        const char* payload = buttonState ? "1" : "0";
        const String base = mqtt.getMqttBaseTopic();
        if (!base.isEmpty()) {
            const String topic = base + "/" + BUTTON_TOPIC;
            mqtt.publishExtraTopicImmediately(BUTTON_ID, topic.c_str(), payload, false);
        }
    }

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

void setupMqtt()
{
    mqtt.attach(ConfigManager);

    // Classic callbacks (mirror PubSubClient signatures)
    // mqtt.onConnected([]() { CM_LOG("[Full-MQTT-Demo][INFO] MQTT connected (classic)"); });
    // mqtt.onDisconnected([]() { CM_LOG("[Full-MQTT-Demo][INFO] MQTT disconnected (classic)"); });
    // mqtt.onMessage([](char* topic, byte* payload, unsigned int length) { /* ... */ });

    // Receive test topics
    mqtt.addMQTTTopicReceiveFloat("boiler_temp_c", "Boiler Temperature", "BoilerSaver/TemperatureBoiler", &boilerTemperatureC, "C", 1, "none", false); // not added to settings GUI
    mqtt.addMQTTTopicReceiveString("boiler_time_remaining", "Boiler Time Remaining", "BoilerSaver/TimeRemaining", &boilerTimeRemaining, "none"); // not added to settings GUI per default
    mqtt.addMQTTTopicReceiveBool("boiler_shower_now", "You Can Shower Now", "BoilerSaver/YouCanShowerNow", &boilerYouCanShowerNow, "none", true);
    mqtt.addMQTTTopicReceiveFloat("powermeter_power_in_w", "Power Meter Power In", "tele/powerMeter/powerMeter/SENSOR", &powerMeterPowerInW, "W", 0, "E320.Power_in", true);

    mqtt.addMQTTTopicReceiveFloat("washing_machine_energy_total", "Washing Machine Energy Total", "tele/tasmota_F0C5AC/SENSOR", &washingMachineEnergyTotal, "kWh", 3, "ENERGY.Total", true);
    mqtt.addMQTTTopicReceiveFloat("washing_machine_energy_yesterday", "Washing Machine Energy Yesterday", "tele/tasmota_1DEE45/SENSOR", &washingMachineEnergyYesterday, "kWh", 3, "ENERGY.Yesterday", true);
    mqtt.addMQTTTopicReceiveInt("solar_limiter_set_value_w", "Solar Limiter Set Value", "SolarLimiter/SetValue", &solarLimiterSetValueW, "W", "none", false);

    mqtt.addMQTTRuntimeProviderToGUI(ConfigManager, "mqtt", 2, 10);
    mqtt.addMQTTReceiveSettingsToGUI(ConfigManager); // Register receive-topic settings in MQTT tab (only addToSettings=true)

    mqtt.subscribeWildcard("tasmota/+/main/error");

    // GUI examples: explicitly opt-in the receive fields
    mqtt.addMQTTTopicTooGUI(ConfigManager, "boiler_temp_c", "MQTT-Received", 1);
    mqtt.addMQTTTopicTooGUI(ConfigManager, "powermeter_power_in_w", "MQTT-Received", 2);
    mqtt.addMQTTTopicTooGUI(ConfigManager, "washing_machine_energy_total", "MQTT-Received", 3);
    mqtt.addMQTTTopicTooGUI(ConfigManager, "washing_machine_energy_yesterday", "MQTT-Received", 4);
    mqtt.addMQTTTopicTooGUI(ConfigManager, "solar_limiter_set_value_w", "MQTT-Received", 5);

    // GUI examples: other infos via runtime provider
    ConfigManager.getRuntime().addRuntimeProvider("mqtt", [](JsonObject& data) {
        data["lastTopic"] = mqtt.getLastTopic();
        data["lastPayload"] = mqtt.getLastPayload();
        data["lastMsgAgeMs"] = mqtt.getLastMessageAgeMs();
        data["washing_machine_energy_total_mwh"] = washingMachineEnergyTotalMWh;
        data["tasmotaLastError"] = tasmotaLastError;
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

    RuntimeFieldMeta tasmotaErrorMeta;
    tasmotaErrorMeta.group = "mqtt";
    tasmotaErrorMeta.key = "tasmotaLastError";
    tasmotaErrorMeta.label = "Tasmota Last Error";
    tasmotaErrorMeta.isString = true;
    tasmotaErrorMeta.order = 30;
    ConfigManager.getRuntime().addRuntimeMeta(tasmotaErrorMeta);
}

// Global WiFi event hooks used by ConfigManager.
void onWiFiConnected()
{
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    CM_LOG("[Full-MQTT-Demo][INFO] Station Mode: http://%s", WiFi.localIP().toString().c_str());
}

void onWiFiDisconnected()
{
    wifiServices.onDisconnected();
}

void onWiFiAPMode()
{
    wifiServices.onAPMode();
    CM_LOG("[Full-MQTT-Demo][INFO] AP Mode: http://%s", WiFi.softAPIP().toString().c_str());
}
