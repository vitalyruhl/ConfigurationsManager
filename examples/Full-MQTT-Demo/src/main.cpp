#include <Arduino.h>

#include <Ticker.h>
#include <WiFi.h>

#include "ConfigManager.h"

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"

// Optional logging module
#include "logging/LoggingManager.h"

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define CM_HAS_WIFI_SECRETS 1
#else
#define CM_HAS_WIFI_SECRETS 0
#endif

// Optional MQTT module (requires PubSubClient in the consuming project)
#define CM_MQTT_NO_DEFAULT_HOOKS
#include "mqtt/MQTTManager.h"
#include "mqtt/MQTTLogOutput.h"

#define VERSION CONFIGMANAGER_VERSION
#ifndef APP_NAME
#define APP_NAME "CM-Full-MQTT-Demo"
#endif

extern ConfigManagerClass ConfigManager;

#ifndef SETTINGS_PASSWORD
#define SETTINGS_PASSWORD ""
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD SETTINGS_PASSWORD
#endif

// Global theme override demo.
// Served via /user_theme.css and auto-injected by the frontend if present.
static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.rw[data-group="mqtt"][data-key="tasmotaLastError"] * { color: #ec36b0 !important; font-weight: 800 !important; }
)CSS";

static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

static cm::MQTTManager &mqtt = cm::MQTTManager::instance();
static cm::MQTTManager::Settings &mqttSettings = mqtt.settings();
static cm::LoggingManager &lmg = cm::LoggingManager::instance();
using LL = cm::LoggingManager::Level;

static constexpr int BUTTON_PIN = 33;
static const char BUTTON_TOPIC[] = "Imediately_send_button_state";
static const char BUTTON_ID[] = "test_topic_bool_send";
static const char TEST_PUBLISH_TOPIC[] = "test_topic_publish_immediately";
static const char TASMOTA_ERRORS_FILTER[] = "/main/error";
static const char SUBSCRIBE_DEMO_TOPIC[] = "demo/subscribe";

// Receive demo values
static float boilerTemperatureC = 0.0f;
static String boilerTimeRemaining;
static bool boilerYouCanShowerNow = false;

static float powerMeterPowerInW = 0.0f;

static float testEnergyTotal = 0.0f;
static float testEnergyYesterday = 0.0f;
static float testEnergyTotalMWh = 0.0f;
static int solarLimiterSetValueW = 0;
static String tasmotaLastError;

static bool buttonState = false;
static bool buttonLastState = true; // Start with "not pressed" to trigger an initial publish on startup.

static Config<bool> mqttSubscribeDemo(ConfigOptions<bool>{
    .key = "MQTTSubDemo",
    .name = "Enable Subscription Demo",
    .category = "MQTT",
    .defaultValue = false,
    .sortOrder = 1,
    .card = "Subscription demo",
    .cardPretty = "Subscription demo",
    .cardOrder = 60});

void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
void setupMqtt();
void Initial_logging();
static void setupNetworkDefaults();
void setupGUI();
void publishImediatelyButtonState();
void applySubscriptionDemo();

namespace cm
{
    void onMQTTConnected()
    {
        lmg.logTag(LL::Info, "MQTT", "Connected");
        if (mqtt.settings().publishTopicBase.get().isEmpty())
        {
            mqtt.settings().publishTopicBase.set(mqtt.settings().clientId.get());
        }
        const String base = mqtt.getMqttBaseTopic();
        if (!base.isEmpty())
        {
            const String statusTopic = base + "/System-Info/status";
            mqtt.publishExtraTopicImmediately("mqtt_status_aus_Main_Callback", statusTopic.c_str(), "online", true);
        }
        const bool ok = mqtt.publishSystemInfoNow(true);
        if (!ok)
        {
            lmg.logTag(LL::Warn, "MQTT", "Failed to publish System-Info (missing base topic or not connected)");
        }
        applySubscriptionDemo();
    }

    void onMQTTDisconnected()
    {
        lmg.logTag(LL::Info, "MQTT", "Disconnected");
    }

    void onMQTTStateChanged(int state)
    {
        auto mqttState = static_cast<MQTTManager::ConnectionState>(state);
        lmg.logTag(LL::Info, "MQTT", "State changed: %s", MQTTManager::mqttStateToString(mqttState));
    }

    void onNewMQTTMessage(const char *topic, const uint8_t *payload, unsigned int length)
    {
        if (!topic || !payload || length == 0)
        {
            return;
        }
        String payloadString(reinterpret_cast<const char *>(payload), length);
        payloadString.trim();
        lmg.logTag(LL::Info, "MQTT", "RX: %s => %s", topic, payloadString.c_str());

        if (String(topic).endsWith(TASMOTA_ERRORS_FILTER))
        {
            tasmotaLastError = String(topic) + " => " + payloadString;
            lmg.logTag(LL::Error, "TASMOTA", "%s", tasmotaLastError.c_str());
        }
    }
} // namespace cm

void setup()
{
    Initial_logging();
    setupMqtt();

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(VERSION);
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);
    ConfigManager.enableBuiltinSystemProvider();

    coreSettings.attachWiFi(ConfigManager);
    coreSettings.attachSystem(ConfigManager);
    coreSettings.attachNtp(ConfigManager);
    ConfigManager.addSetting(&mqttSubscribeDemo);

    ConfigManager.checkSettingsForErrors();
    ConfigManager.loadAll();

    setupNetworkDefaults();
    setupGUI();

    pinMode(BUTTON_PIN, INPUT_PULLUP);

// Prefer selected AP when provided in secret file.
#if defined(WIFI_FILTER_MAC_PRIORITY)
    ConfigManager.setAccessPointMacPriority(WIFI_FILTER_MAC_PRIORITY);
#endif

    // Settings-driven WiFi startup (DHCP/static/AP fallback).
    ConfigManager.startWebServer();

    mqtt.clearRetain("test_topic_publish_immediately");
    mqtt.publishAllNow();

    mqtt.publishExtraTopicImmediately("test_topic_publish_immediately", TEST_PUBLISH_TOPIC, "1", false);
    mqtt.publishTopicImmediately("solar_limiter_set_value_w");
    lmg.logTag(LL::Info, "SETUP", "Completed successfully. Starting main loop...");
}

void loop()
{
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();

    mqtt.loop();
    lmg.loop();

    buttonState = (digitalRead(BUTTON_PIN) == LOW);
    publishImediatelyButtonState();
    publishNonImediateMQTTToppcs();

    static unsigned long lastLoopLogMs = 0;
    const unsigned long now = millis();
    sendAllMQTTLog(now, lastLoopLogMs);

    delay(10);
}

void setupMqtt()
{
    mqtt.attach(ConfigManager);

    // Classic callbacks (mirror PubSubClient signatures)
    // mqtt.onConnected([]() { CM_LOG("[Full-MQTT-Demo][INFO] MQTT connected (classic)"); });
    // mqtt.onDisconnected([]() { CM_LOG("[Full-MQTT-Demo][INFO] MQTT disconnected (classic)"); });
    // mqtt.onMessage([](const char* topic, const byte* payload, unsigned int length) { /* ... */ });

    // Receive test topics
    mqtt.addTopicReceiveFloat("boiler_temp_c", "Boiler Temperature", "BoilerSaver/TemperatureBoiler", &boilerTemperatureC, "C", 1, "none");  // no settings UI
    mqtt.addTopicReceiveString("boiler_time_remaining", "Boiler Time Remaining", "BoilerSaver/TimeRemaining", &boilerTimeRemaining, "none"); // no settings UI
    mqtt.addTopicReceiveBool("boiler_shower_now", "You Can Shower Now", "BoilerSaver/YouCanShowerNow", &boilerYouCanShowerNow, "none");
    mqtt.addTopicReceiveFloat("powermeter_power_in_w", "Power Meter Power In", "tele/powerMeter/powerMeter/SENSOR", &powerMeterPowerInW, "W", 0, "E320.Power_in");

    mqtt.addTopicReceiveFloat("test_energy_total", "Test Energy Total", "tele/tasmota_F0C5AC/SENSOR", &testEnergyTotal, "kWh", 3, "ENERGY.Total");
    mqtt.addTopicReceiveFloat("test_energy_yesterday", "Test Energy Yesterday", "tele/tasmota_1DEE45/SENSOR", &testEnergyYesterday, "kWh", 3, "ENERGY.Yesterday");
    mqtt.addTopicReceiveInt("solar_limiter_set_value_w", "Solar Limiter Set Value", "SolarLimiter/SetValue", &solarLimiterSetValueW, "W", "none");

    // register a wildcard subscription for all Tasmota errors (topics ending with /main/error) without a specific receive item (handled in onNewMQTTMessage callback)
    mqtt.subscribeWildcard("tasmota/+/main/error");

    mqttSubscribeDemo.setCallback([](bool) { applySubscriptionDemo(); });
}

void setupGUI()
{
    // add to settings
    mqtt.addMqttSettingsToSettingsGroup(ConfigManager, "MQTT", "MQTT Settings", 40);
    const char *mqttTopicsPage = "MQTT-Topics";
    const char *mqttTopicsCard = "MQTT-Topics-Card";
    const char *mqttTopicsGroup = "MQTT-Received";
    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "boiler_shower_now", mqttTopicsPage, mqttTopicsCard, mqttTopicsGroup, 50);
    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "powermeter_power_in_w", mqttTopicsPage, mqttTopicsCard, mqttTopicsGroup, 50);
    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "test_energy_total", mqttTopicsPage, mqttTopicsCard, mqttTopicsGroup, 50);
    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "test_energy_yesterday", mqttTopicsPage, mqttTopicsCard, mqttTopicsGroup, 50);

    // add to live values (over mqtt shorthand)
    const char *livePage = "MQTT";
    const char *liveReceivedCard = "MQTT-Received";
    const char *liveReceivedGroup = "Grouped receive values";                                             //"MQTT-Received";
    mqtt.addMqttTopicToLiveGroup(ConfigManager, "boiler_shower_now", livePage, liveReceivedCard, 1);      // and witout Group
    mqtt.addMqttTopicToLiveGroup(ConfigManager, "boiler_temp_c", livePage, liveReceivedCard, nullptr, 1); // not in Settings!  and witout Group
    mqtt.addMqttTopicToLiveGroup(ConfigManager, "powermeter_power_in_w", livePage, liveReceivedCard, liveReceivedGroup, 2);
    mqtt.addMqttTopicToLiveGroup(ConfigManager, "test_energy_total", livePage, liveReceivedCard, liveReceivedGroup, 3); // not in Settings!
    mqtt.addMqttTopicToLiveGroup(ConfigManager, "test_energy_yesterday", livePage, liveReceivedCard, liveReceivedGroup, 4);
    mqtt.addMqttTopicToLiveGroup(ConfigManager, "solar_limiter_set_value_w", livePage, liveReceivedCard, liveReceivedGroup, 5); // not in Settings!

    // GUI examples: other infos via runtime values
    auto mqttOther = ConfigManager.liveGroup("mqtt")
                         .page(livePage)
                         .card("MQTT Other Infos");

    mqttOther.value("baseTopic", []()
                    { return mqtt.getMqttBaseTopic(); })
        .label("Base Topic")
        .order(1);

    mqttOther.value("lastMsgAgeMs", []()
                    { return mqtt.getLastMessageAgeMs(); })
        .label("Last Message Age")
        .unit("ms")
        .order(20);

    mqttOther.value("lastPayload", []()
                    { return mqtt.getLastPayload(); })
        .label("Last Payload")
        .order(21);

    mqttOther.value("lastTopic", []()
                    { return mqtt.getLastTopic(); })
        .label("Last Topic")
        .order(22);

    mqttOther.value("tasmotaLastError", []()
                    { return tasmotaLastError; })
        .label("Tasmota Last Error")
        .order(30);

    auto mqttReceived = ConfigManager.liveGroup("mqtt")
                            .page(livePage)
                            .card(liveReceivedCard);
    //.group(liveReceivedGroup);

    mqttReceived.value("test_energy_total_mwh", []()
                       { return testEnergyTotalMWh; })
        .label("Test Energy Total")
        .unit("MWh")
        .precision(2)
        .order(4);

    mqttReceived.value("buttonState", []()
                       { return buttonState; })
        .label("Button State")
        .order(5);
}

void Initial_logging()
{
    Serial.begin(115200);
    auto serialOut = std::make_unique<cm::LoggingManager::SerialOutput>(Serial);
    serialOut->setLevel(LL::Debug);
    serialOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::Millis);
    serialOut->setRateLimitMs(2);
    lmg.addOutput(std::move(serialOut));

    lmg.setGlobalLevel(LL::Debug);
    auto scopedSetup = lmg.scopedTag("SETUP");
    lmg.attachToConfigManager(LL::Debug, LL::Debug, "");

    //------------------------------------------------------------------------------------
    // Add MQTT log output (optional)
    auto mqttLog = std::make_unique<cm::MQTTLogOutput>(mqtt);
    mqttLog->setLevel(LL::Debug);
    mqttLog->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
    lmg.addOutput(std::move(mqttLog));

    // Example: custom retained log topic for tags that start with "Custom"
    // This shows how to add a dedicated log output without changing the core logger.
    {
        auto customLog = std::make_unique<cm::MQTTLogOutput>(mqtt);
        customLog->setLevel(LL::Debug);
        customLog->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
        customLog->setRateLimitMs(50);
        customLog->setUnretainedEnabled(false);
        customLog->setRetainedLevels(false, false, false);
        customLog->setCustomTagPrefix("Custom");
        customLog->setCustomRetainedEnabled(true);
        customLog->setFilter([](LL, const char *tag, const char *)
                             { return tag && strncmp(tag, "Custom", 6) == 0; });
        lmg.addOutput(std::move(customLog));
    }
}

// Global WiFi event hooks used by ConfigManager.
void onWiFiConnected()
{
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    lmg.logTag(LL::Info, "WiFi", "Station Mode: http://%s", WiFi.localIP().toString().c_str());
}

void onWiFiDisconnected()
{
    wifiServices.onDisconnected();
    lmg.logTag(LL::Warn, "WiFi", "Disconnected");
}

void onWiFiAPMode()
{
    wifiServices.onAPMode();
    lmg.logTag(LL::Info, "WiFi", "AP Mode: http://%s", WiFi.softAPIP().toString().c_str());
}

//----------------------------------------
// Other FUNCTIONS
//----------------------------------------

void publishImediatelyButtonState()
{
    if (buttonState == buttonLastState)
    {
        return;
    }

    buttonLastState = buttonState;
    const char *payload = buttonState ? "1" : "0";
    const String base = mqtt.getMqttBaseTopic();
    if (base.isEmpty())
    {
        lmg.logTag(LL::Warn, "LOOP", "MQTT base topic empty");
        return;
    }
    if (!BUTTON_ID || !BUTTON_TOPIC)
    {
        lmg.logTag(LL::Warn, "LOOP", "MQTT button topic/id empty");
        return;
    }

    const String topic = base + "/" + BUTTON_TOPIC;
    mqtt.publishExtraTopicImmediately(BUTTON_ID, topic.c_str(), payload, false);
}

void publishNonImediateMQTTToppcs()
{
    testEnergyTotalMWh = testEnergyTotal / 1000.0f;
    // Non-immediate publish via built-in rate limiting (Publish Interval)
    mqtt.publishTopic("test_energy_total", true); // retained example (QoS 0 default)
    mqtt.publishTopic("test_energy_yesterday");
    mqtt.publishTopic("solar_limiter_set_value_w");

    // Extra-topic example (non-immediate, custom topic path)
    const String base = mqtt.getMqttBaseTopic();
    if (!base.isEmpty())
    {
        char payload[16];
        snprintf(payload, sizeof(payload), "%.2f", static_cast<double>(testEnergyTotalMWh));
        const String topic = base + "/extra/test_energy_total_mwh";
        mqtt.publishExtraTopic("test_energy_total_mwh", topic.c_str(), payload, true);
    }
}

void applySubscriptionDemo()
{
    if (!mqtt.isConnected())
    {
        return;
    }
    if (mqttSubscribeDemo.get())
    {
        mqtt.subscribe(SUBSCRIBE_DEMO_TOPIC, 0);
    }
    else
    {
        mqtt.unsubscribe(SUBSCRIBE_DEMO_TOPIC);
    }
}

void sendAllMQTTLog(const unsigned long now, unsigned long &lastLoopLogMs)
{
    if (now - lastLoopLogMs >= 10000)
    {

        static int randomValue = 0;
        randomValue = static_cast<int>(random(0, 10));

        lastLoopLogMs = now;
        lmg.logTag(LL::Info, "Loop", "wifi=%s mqtt=%s base=%s lastTopic=%s",
                   WiFi.isConnected() ? "connected" : "disconnected",
                   mqtt.isConnected() ? "connected" : "disconnected",
                   mqtt.getMqttBaseTopic().c_str(),
                   mqtt.getLastTopic().c_str());

        lmg.logTag(LL::Fatal, "Loop", "Fatal example (value=%d)", randomValue);
        lmg.logTag(LL::Error, "Loop", "Error example (value=%d)", randomValue);
        lmg.logTag(LL::Warn, "Loop", "Warn example (value=%d)", randomValue);
        lmg.logTag(LL::Info, "Loop", "Info example (value=%d)", randomValue);
        lmg.logTag(LL::Debug, "Loop", "Debug example (value=%d)", randomValue);
        lmg.logTag(LL::Trace, "Loop", "Trace example (value=%d)", randomValue);
    }
}

static void setupNetworkDefaults()
{
    if (wifiSettings.wifiSsid.get().isEmpty())
    {
#if CM_HAS_WIFI_SECRETS
        Serial.println("-------------------------------------------------------------");
        Serial.println("SETUP: *** SSID is empty, setting My values *** ");
        Serial.println("-------------------------------------------------------------");
        wifiSettings.wifiSsid.set(MY_WIFI_SSID);
        wifiSettings.wifiPassword.set(MY_WIFI_PASSWORD);

        // Optional secret fields (not present in every example).
#ifdef MY_WIFI_IP
        wifiSettings.staticIp.set(MY_WIFI_IP);
#endif
#ifdef MY_USE_DHCP
        wifiSettings.useDhcp.set(MY_USE_DHCP);
#endif
#ifdef MY_GATEWAY_IP
        wifiSettings.gateway.set(MY_GATEWAY_IP);
#endif
#ifdef MY_SUBNET_MASK
        wifiSettings.subnet.set(MY_SUBNET_MASK);
#endif
#ifdef MY_DNS_IP
        wifiSettings.dnsPrimary.set(MY_DNS_IP);
#endif
        ConfigManager.saveAll();
        Serial.println("-------------------------------------------------------------");
        Serial.println("Restarting ESP, after auto setting WiFi credentials");
        Serial.println("-------------------------------------------------------------");
        delay(500);
        ESP.restart();
#else
        Serial.println("SETUP: WiFi SSID is empty but secret/secrets.h is missing; using UI/AP mode");
#endif
    }

    if (mqttSettings.server.get().isEmpty())
    {
#if CM_HAS_WIFI_SECRETS
#if defined(MY_MQTT_BROKER_IP) && defined(MY_MQTT_BROKER_PORT) && defined(MY_MQTT_ROOT)
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        lmg.log(LL::Debug, "SETUP: *** MQTT Broker is empty, setting My values *** ");
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        mqttSettings.server.set(MY_MQTT_BROKER_IP);
        mqttSettings.port.set(MY_MQTT_BROKER_PORT);
#ifdef MY_MQTT_USERNAME
        mqttSettings.username.set(MY_MQTT_USERNAME);
#endif
#ifdef MY_MQTT_PASSWORD
        mqttSettings.password.set(MY_MQTT_PASSWORD);
#endif
        mqttSettings.publishTopicBase.set(MY_MQTT_ROOT);
        ConfigManager.saveAll();
        lmg.log(LL::Debug, "-------------------------------------------------------------");
#else
        lmg.log(LL::Info, "SETUP: MQTT server is empty; secret/secrets.h does not provide MQTT defaults for this example");
#endif
#else
        lmg.log(LL::Info, "SETUP: MQTT server is empty and secret/secrets.h is missing; leaving MQTT unconfigured");
#endif
    }

    if (systemSettings.otaPassword.get() != OTA_PASSWORD)
    {
        systemSettings.otaPassword.save(OTA_PASSWORD);
    }
}
