#include <Arduino.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <BME280_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// AsyncWebServer instance is provided by ConfigManager library (extern AsyncWebServer server)

#include "ConfigManager.h"

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "io/IOManager.h"
#include "alarm/AlarmManager.h"
#include "logging/LoggingManager.h"

#define CM_MQTT_NO_DEFAULT_HOOKS
#include "mqtt/MQTTManager.h"
#include "mqtt/MQTTLogOutput.h"

#include "settings_v3.h"
#include "RS485Module/RS485Module.h"
#include "helpers/HelperModule.h"
#include "Smoother/Smoother.h"

#if __has_include("secret/wifiSecret.h")
#include "secret/wifiSecret.h"
#define CM_HAS_WIFI_SECRETS 1
#else
#define CM_HAS_WIFI_SECRETS 0
#endif

#ifndef APMODE_SSID
#define APMODE_SSID "ESP32_Config"
#endif

#ifndef APMODE_PASSWORD
#define APMODE_PASSWORD "config1234"
#endif

// predeclare the functions (prototypes)
void SetupStartDisplay();
void cb_RS485Listener();
void testRS232();
void readBme280();
void WriteToDisplay();
void SetupStartTemperatureMeasuring();
void ProjectConfig();
void CheckVentilator(float currentTemperature);
void EvaluateHeater(float currentTemperature);
void ShowDisplayOn();
void ShowDisplayOff();
static void logNetworkIpInfo(const char *context);
void setupGUI();
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
static void setupLogging();
static void setupMqtt();
static void registerIOBindings();
static void updateMqttTopics();
static void publishMqttNow();
static void publishMqttNowIfNeeded();
static void setFanRelay(bool on);
static void setHeaterRelay(bool on);
static void setManualOverride(bool on);
static bool getFanRelay();
static bool getHeaterRelay();
//--------------------------------------------------------------------------------------------------------------

#pragma region configuration variables

BME280_I2C bme280;

static constexpr int OLED_WIDTH = 128;
static constexpr int OLED_HEIGHT = 32;
static constexpr int OLED_RESET_PIN = 4; // keep legacy wiring default
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);

Ticker RS485Ticker;
Ticker temperatureTicker;
Ticker displayTicker;

Smoother* powerSmoother = nullptr; //there is a memory allocation in setup, better use a pointer here

// global helper variables
int currentGridImportW = 0; // amount of electricity being imported from grid
int inverterSetValue = 0;     // current power inverter should deliver (default to zero)
int solarPowerW = 0;          // current solar production
float temperature = 0.0;      // current temperature in Celsius
float Dewpoint = 0.0;         // current dewpoint in Celsius
float Humidity = 0.0;         // current humidity in percent
float Pressure = 0.0;         // current pressure in hPa

bool displayActive = true;   // flag to indicate if the display is active
static bool displayInitialized = false;
static bool dewpointRiskActive = false; // tracks dewpoint alarm state
static bool heaterLatchedState = false;  // hysteresis latch for heater
static bool manualOverrideActive = false; // when enabled, buttons control relays and automation pauses

static inline ConfigManagerRuntime &CRM() { return ConfigManager.getRuntime(); } // Shorthand helper for RuntimeManager access

static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.rw[data-group="sensors"][data-key="temp"]{ color:rgb(198, 16, 16) !important; font-weight:900; font-size: 1.2rem; }
.rw[data-group="sensors"][data-key="temp"] *{ color:rgb(198, 16, 16) !important; font-weight:900; font-size: 1.2rem; }
)CSS";

static cm::LoggingManager &lmg = cm::LoggingManager::instance();
using LL = cm::LoggingManager::Level;
static cm::MQTTManager &mqtt = cm::MQTTManager::instance();
static cm::MQTTManager::Settings &mqttSettings = mqtt.settings();
static cm::IOManager ioManager;
static cm::AlarmManager alarmManager;

static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

static constexpr char IO_FAN_ID[] = "fan_relay";
static constexpr char IO_HEATER_ID[] = "heater_relay";
static constexpr char IO_RESET_ID[] = "reset_btn";
static constexpr char IO_AP_ID[] = "ap_btn";

static String mqttBaseTopic;
static String topicPublishSetValueW;
static String topicPublishGridImportW;
static String topicPublishTempC;
static String topicPublishHumidityPct;
static String topicPublishDewpointC;
static unsigned long lastMqttPublishMs = 0;
static unsigned long lastSolarTraceMs = 0;

#pragma endregion configurationn variables

//----------------------------------------
// MAIN FUNCTIONS
//----------------------------------------

static void logNetworkIpInfo(const char *context)
{
  const WiFiMode_t mode = WiFi.getMode();
  const bool apActive = (mode == WIFI_AP) || (mode == WIFI_AP_STA);
  const bool staConnected = (WiFi.status() == WL_CONNECTED);

  if (apActive)
  {
    const IPAddress apIp = WiFi.softAPIP();
    lmg.logTag(LL::Debug, "WiFi", "%s: AP IP: %s", context, apIp.toString().c_str());
    lmg.logTag(LL::Debug, "WiFi", "%s: AP SSID: %s", context, WiFi.softAPSSID().c_str());
  }

  if (staConnected)
  {
    const IPAddress staIp = WiFi.localIP();
    lmg.logTag(LL::Debug, "WiFi", "%s: STA IP: %s", context, staIp.toString().c_str());
  }
}

void setup()
{
    setupLogging();
    lmg.scopedTag("SETUP");
    lmg.log("System setup start...");

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(VERSION);
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);
    ConfigManager.enableBuiltinSystemProvider();

    // coreSettings owns the WiFi/System/NTP pages now; MQTT module registers its own layout.
    ConfigManager.addSettingsPage("Limiter", 60);
    ConfigManager.addSettingsGroup("Limiter", "Limiter", "Limiter Settings", 60);
    ConfigManager.addSettingsPage("Temp", 70);
    ConfigManager.addSettingsGroup("Temp", "Temp", "Temp Settings", 70);
    ConfigManager.addSettingsPage("I2C", 80);
    ConfigManager.addSettingsGroup("I2C", "I2C", "I2C Settings", 80);
    ConfigManager.addSettingsPage("Fan", 90);
    ConfigManager.addSettingsGroup("Fan", "Fan", "Fan Settings", 90);
    ConfigManager.addSettingsPage("Heater", 100);
    ConfigManager.addSettingsGroup("Heater", "Heater", "Heater Settings", 100);
    ConfigManager.addSettingsPage("Display", 110);
    ConfigManager.addSettingsGroup("Display", "Display", "Display Settings", 110);
    ConfigManager.addSettingsPage("RS485", 120);
    ConfigManager.addSettingsGroup("RS485", "RS485", "RS485 Settings", 120);
    ConfigManager.addSettingsPage("I/O", 130);
    ConfigManager.addSettingsGroup("I/O", "I/O", "I/O Settings", 130);

    ConfigManager.addLivePage("sensors", 10);
    ConfigManager.addLiveGroup("sensors", "Live Values", "Sensor Readings", 10);
    ConfigManager.addLivePage("Limiter", 20);
    ConfigManager.addLiveGroup("Limiter", "Live Values", "Limiter Status", 20);
    ConfigManager.addLivePage("Outputs", 30);
    ConfigManager.addLiveGroup("Outputs", "Live Values", "Relay Status", 30);
    ConfigManager.addLivePage("controls", 40);
    ConfigManager.addLiveGroup("controls", "Live Controls", "Manual Controls", 40);

    coreSettings.attachWiFi(ConfigManager);
    coreSettings.attachSystem(ConfigManager);
    coreSettings.attachNtp(ConfigManager);

    initializeAllSettings();
    registerIOBindings();
    setupMqtt();

    ConfigManager.checkSettingsForErrors();
    ConfigManager.loadAll();
    delay(100);

    // Apply secret defaults only if nothing is configured yet (after loading persisted settings).
    if (wifiSettings.wifiSsid.get().isEmpty())
    {
#if CM_HAS_WIFI_SECRETS
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        lmg.log(LL::Debug, "SETUP: *** SSID is empty, setting My values *** ");
        lmg.log(LL::Debug, "-------------------------------------------------------------");
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
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        lmg.log(LL::Debug, "Restarting ESP, after auto setting WiFi credentials");
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        delay(500);
        ESP.restart();
#else
        lmg.log(LL::Warn, "SETUP: WiFi SSID is empty but secret/wifiSecret.h is missing; using UI/AP mode");
#endif
    }

    mqtt.attach(ConfigManager);// Re-attach to apply loaded values (attach() is idempotent)
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
        lmg.log(LL::Info, "SETUP: MQTT server is empty; secret/wifiSecret.h does not provide MQTT defaults for this example");
#endif
#else
        lmg.log(LL::Info, "SETUP: MQTT server is empty and secret/wifiSecret.h is missing; leaving MQTT unconfigured");
#endif
    }

    systemSettings.allowOTA.setCallback(
      [](bool enabled){
            lmg.logTag(LL::Info, "OTA", "Setting changed to: %s", enabled ? "enabled" : "disabled");
            ConfigManager.getOTAManager().enable(enabled);
          });

    ConfigManager.getOTAManager().enable(systemSettings.allowOTA.get());

    ioManager.begin();

    // Apply WiFi reboot timeout from settings (minutes)

    ConfigManager.startWebServer();


    ConfigManager.enableSmartRoaming(true);
    ConfigManager.setRoamingThreshold(-75);
    ConfigManager.setRoamingCooldown(30);
    ConfigManager.setRoamingImprovement(10);

    // Prefer this AP, fallback to others
    // ConfigManager.setWifiAPMacPriority("3C:A6:2F:B8:54:B1");//shest
    ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");// dev-Station

    updateMqttTopics();
    setupGUI();
    SetupStartDisplay();
    ShowDisplayOn();

    cm::helpers::pulseWait(LED_BUILTIN, cm::helpers::PulseOutput::ActiveLevel::ActiveHigh, 3, 100);

    powerSmoother = new Smoother(
        limiterSettings.smoothingSize->get(),
        limiterSettings.inputCorrectionOffset->get(),
        limiterSettings.minOutput->get(),
        limiterSettings.maxOutput->get());
    powerSmoother->fillBufferOnStart(limiterSettings.minOutput->get());

    RS485begin();
    SetupStartTemperatureMeasuring();

    RS485Ticker.attach(limiterSettings.RS232PublishPeriod->get(), cb_RS485Listener);


    setFanRelay(false);
    setHeaterRelay(false);

    lmg.logTag(LL::Info, "SETUP", "Completed successfully. Starting main loop...");
}

void loop()
{
    ConfigManager.getWiFiManager().update();
    lmg.loop();
    ioManager.update();

    // Services managed by ConfigManager.
    ConfigManager.handleClient();
    ConfigManager.handleOTA();
    alarmManager.update();

    if (mqttSettings.enableMQTT.get() && ConfigManager.getWiFiManager().isConnected() && !ConfigManager.getWiFiManager().isInAPMode())
    {
        mqtt.loop();

        const String &lastTopic = mqtt.getLastTopic();
        if (lastTopic.equalsIgnoreCase("tele/tasmota_1DEE45/SENSOR"))
        {
            const String &payload = mqtt.getLastPayload();
            lmg.logTag(LL::Trace, "MQTT", "Solar topic received: %s | payload: %s", lastTopic.c_str(), payload.c_str());
            lastSolarTraceMs = millis();
        }

        publishMqttNowIfNeeded();
    }

    // Status LED: simple feedback
    if (ConfigManager.getWiFiManager().isInAPMode())
    {
        digitalWrite(LED_BUILTIN, HIGH);
    }
    else if (ConfigManager.getWiFiManager().isConnected() && mqtt.isConnected())
    {
        digitalWrite(LED_BUILTIN, LOW);
    }
    else
    {
        digitalWrite(LED_BUILTIN, HIGH);
    }

    WriteToDisplay();

    if (!manualOverrideActive)
    {
        CheckVentilator(temperature);
        EvaluateHeater(temperature);
    }
    delay(10);
}

void setupGUI()
{
  // region sensor fields BME280
    CRM().addRuntimeProvider("sensors", [](JsonObject &data)
      {
          // Apply precision to sensor values to reduce JSON size
          data["temp"] = roundf(temperature * 10.0f) / 10.0f;     // 1 decimal place
          data["hum"] = roundf(Humidity * 10.0f) / 10.0f;        // 1 decimal place
          data["dew"] = roundf(Dewpoint * 10.0f) / 10.0f;        // 1 decimal place
          data["pressure"] = roundf(Pressure * 10.0f) / 10.0f;   // 1 decimal place
    }, 2);


      // Define sensor display fields using addRuntimeMeta
      RuntimeFieldMeta tempMeta;
      tempMeta.group = "sensors";
      tempMeta.key = "temp";
      tempMeta.label = "Temperature";
      tempMeta.unit = "°C";
      tempMeta.precision = 1;
      tempMeta.order = 2;
      CRM().addRuntimeMeta(tempMeta);

      RuntimeFieldMeta humMeta;
      humMeta.group = "sensors";
      humMeta.key = "hum";
      humMeta.label = "Humidity";
      humMeta.unit = "%";
      humMeta.precision = 1;
      humMeta.order = 11;
      CRM().addRuntimeMeta(humMeta);

      RuntimeFieldMeta dewMeta;
      dewMeta.group = "sensors";
      dewMeta.key = "dew";
      dewMeta.label = "Dewpoint";
      dewMeta.unit = "°C";
      dewMeta.precision = 1;
      dewMeta.order = 12;
      CRM().addRuntimeMeta(dewMeta);

      RuntimeFieldMeta pressureMeta;
      pressureMeta.group = "sensors";
      pressureMeta.key = "pressure";
      pressureMeta.label = "Pressure";
      pressureMeta.unit = "hPa";
      pressureMeta.precision = 1;
      pressureMeta.order = 13;
      CRM().addRuntimeMeta(pressureMeta);


      RuntimeFieldMeta rangeMeta;
      rangeMeta.group = "sensors";
      rangeMeta.key = "range";
      rangeMeta.label = "Sensor Range";
      rangeMeta.unit = "V";
      rangeMeta.precision = 1;
      rangeMeta.order = 14;
      CRM().addRuntimeMeta(rangeMeta);
    // endregion sensor fields


  //region Limiter
      CRM().addRuntimeProvider("Limiter", [](JsonObject &data)
      {
          // Apply precision to sensor values to reduce JSON size
          data["gridIn"] = currentGridImportW;
          data["invSet"] = inverterSetValue;
          data["solar"] = solarPowerW;
          data["enabled"] = limiterSettings.enableController->get();
      }, 1);

      // Define sensor display fields using addRuntimeMeta
      RuntimeFieldMeta gridInMeta;
      gridInMeta.group = "Limiter";
      gridInMeta.key = "gridIn";
      gridInMeta.label = "Grid Import";
      gridInMeta.unit = "W";
      gridInMeta.precision = 0;
      gridInMeta.order = 2;
      CRM().addRuntimeMeta(gridInMeta);

      RuntimeFieldMeta invSetMeta;
      invSetMeta.group = "Limiter";
      invSetMeta.key = "invSet";
      invSetMeta.label = "Inverter Setpoint";
      invSetMeta.unit = "W";
      invSetMeta.precision = 0;
      invSetMeta.order = 3;
      CRM().addRuntimeMeta(invSetMeta);

      RuntimeFieldMeta limiterEnabledMeta;
      limiterEnabledMeta.group = "Limiter";
      limiterEnabledMeta.key = "enabled";
      limiterEnabledMeta.label = "Limiter Enabled";
      limiterEnabledMeta.isBool = true;
      limiterEnabledMeta.order = 1;
      CRM().addRuntimeMeta(limiterEnabledMeta);

      RuntimeFieldMeta solarMeta;
      solarMeta.group = "Limiter";
      solarMeta.key = "solar";
      solarMeta.label = "Solar power";
      solarMeta.unit = "W";
      solarMeta.precision = 0;
      solarMeta.order = 4;
      CRM().addRuntimeMeta(solarMeta);
  //endregion Limiter


  //region relay outputs
      CRM().addRuntimeProvider("Outputs", [](JsonObject &data)
      {
          data["ventilator"] = getFanRelay();
          data["heater"] = getHeaterRelay();
          data["dewpoint_risk"] = dewpointRiskActive;
          data["manual_override"] = manualOverrideActive;
      }, 3);

      CRM().defineRuntimeCheckbox(
          "Outputs",
          "manual_override",
          "Manual Override",
          []() { return manualOverrideActive; },
          [](bool on) { setManualOverride(on); },
          String(),   // optional Hilfetext
          0);

      CRM().defineRuntimeStateButton(
          "Outputs",
          "ventilator",
          "Ventilator Relay",
          []() { return getFanRelay(); },
          [](bool on) { setFanRelay(on); },
          getFanRelay(),
          String(),
          1,
          "On",
          "Off");

      CRM().defineRuntimeStateButton(
          "Outputs",
          "heater",
          "Heater Relay",
          []() { return getHeaterRelay(); },
          [](bool on) { setHeaterRelay(on); },
          getHeaterRelay(),
          String(),
          2,
          "On",
          "Off");

      // RuntimeFieldMeta manualOverrideMeta;
      // manualOverrideMeta.group = "Outputs";
      // manualOverrideMeta.key = "manual_override";
      // manualOverrideMeta.label = "Manual Override";
      // manualOverrideMeta.isBool = true;
      // manualOverrideMeta.order = 0;
      // CRM().addRuntimeMeta(manualOverrideMeta);

      RuntimeFieldMeta ventilatorMeta;
      ventilatorMeta.group = "Outputs";
      ventilatorMeta.key = "ventilator";
      ventilatorMeta.label = "Ventilator Relay Active";
      ventilatorMeta.isBool = true;
      ventilatorMeta.order = 1;
      CRM().addRuntimeMeta(ventilatorMeta);

      RuntimeFieldMeta heaterMeta;
      heaterMeta.group = "Outputs";
      heaterMeta.key = "heater";
      heaterMeta.label = "Heater Relay Active";
      heaterMeta.isBool = true;
      heaterMeta.order = 2;
      CRM().addRuntimeMeta(heaterMeta);

      alarmManager.addDigitalWarning(
          {
              .id = "dewpoint_risk",
              .name = "Dewpoint Risk",
              .kind = cm::AlarmKind::DigitalActive,
              .severity = cm::AlarmSeverity::Warning,
              .enabled = true,
              .getter = []() { return (temperature - Dewpoint) <= tempSettings.dewpointRiskWindow->get(); },
          })
          .onAlarmCome([]() {
              dewpointRiskActive = true;
              lmg.logTag(LL::Warn, "ALARM", "Dewpoint risk ENTER");
              EvaluateHeater(temperature);
          })
          .onAlarmGone([]() {
              dewpointRiskActive = false;
              lmg.logTag(LL::Info, "ALARM", "Dewpoint risk EXIT");
              EvaluateHeater(temperature);
          });
      alarmManager.addWarningToLive("dewpoint_risk", 3, "Outputs", "Live Values", "Outputs", "Dewpoint Risk");
  //endregion relay outputs

}

//----------------------------------------
// LOGGING / IO / MQTT SETUP
//----------------------------------------

static void setupLogging()
{
    Serial.begin(115200);

    auto serialOut = std::make_unique<cm::LoggingManager::SerialOutput>(Serial);
    serialOut->setLevel(LL::Trace);
    serialOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::Millis);
    serialOut->setRateLimitMs(2);
    lmg.addOutput(std::move(serialOut));

    lmg.setGlobalLevel(LL::Trace);
    lmg.attachToConfigManager(LL::Info, LL::Trace, "");


}

static void registerIOBindings()
{
    lmg.scopedTag("IO");
    analogReadResolution(12);

    ioManager.addDigitalOutput(IO_FAN_ID, "Cooling Fan Relay", 23, true, true);
    ioManager.addDigitalOutputToSettingsGroup(IO_FAN_ID, "I/O", "Cooling Fan Relay", "Cooling Fan Relay", 1);

    ioManager.addDigitalOutput(IO_HEATER_ID, "Heater Relay", 27, true, true);
    ioManager.addDigitalOutputToSettingsGroup(IO_HEATER_ID, "I/O", "Heater Relay", "Heater Relay", 2);

    ioManager.addDigitalInput(IO_RESET_ID, "Reset Button", 14, true, true, false, true);
    ioManager.addDigitalInputToSettingsGroup(IO_RESET_ID, "I/O", "Reset Button", "Reset Button", 10);

    ioManager.addDigitalInput(IO_AP_ID, "AP Mode Button", 13, true, true, false, true);
    ioManager.addDigitalInputToSettingsGroup(IO_AP_ID, "I/O", "AP Mode Button", "AP Mode Button", 11);

    cm::IOManager::DigitalInputEventOptions resetOptions;
    resetOptions.longClickMs = 3000;
    ioManager.configureDigitalInputEvents(
        IO_RESET_ID,
        cm::IOManager::DigitalInputEventCallbacks{
            .onPress = []() {
                lmg.logTag(LL::Debug, "IO", "Reset button pressed -> show display");
                ShowDisplayOn();
            },
            .onLongPressOnStartup = []() {
                lmg.logTag(LL::Warn, "IO", "Reset button pressed at startup -> restoring defaults");
                ConfigManager.clearAllFromPrefs();
                ConfigManager.saveAll();
                delay(500);
                ESP.restart();
            },
        },
        resetOptions);

    cm::IOManager::DigitalInputEventOptions apOptions;
    apOptions.longClickMs = 1200;
    ioManager.configureDigitalInputEvents(
        IO_AP_ID,
        cm::IOManager::DigitalInputEventCallbacks{
            .onPress = []() {
                lmg.logTag(LL::Debug, "IO", "AP button pressed -> show display");
                ShowDisplayOn();
            },
            .onLongPressOnStartup = []() {
                lmg.logTag(LL::Warn, "IO", "AP button pressed at startup -> starting AP mode");
                ConfigManager.startAccessPoint(APMODE_SSID, APMODE_PASSWORD);
            },
        },
        apOptions);
}

static void setupMqtt()
{
    mqtt.attach(ConfigManager);
    mqtt.addMqttSettingsToSettingsGroup(ConfigManager, "MQTT", "MQTT Settings", 40);

    // Receive: grid import W (from power meter JSON)
    mqtt.addTopicReceiveInt(
        "grid_import_w",
        "Grid Import",
        "tele/powerMeter/powerMeter/SENSOR",
        &currentGridImportW,
        "W",
        "E320.Power_in");

    mqtt.addTopicReceiveInt(
        "solar_power_w",
        "Solar power",
        "tele/tasmota_1DEE45/SENSOR",
        &solarPowerW,
        "W",
        "ENERGY.Power");

    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "grid_import_w", "MQTT-Topics", "MQTT-Topics", "MQTT-Received", 50);
    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "solar_power_w", "MQTT-Topics", "MQTT-Topics", "MQTT-Received", 50);

    mqtt.onConnected([](){
        const char* topic = "tele/tasmota_1DEE45/SENSOR";
        const bool ok = mqtt.subscribe(topic);
        lmg.logTag(LL::Debug, "MQTT", "Subscribed to solar topic %s -> %s", topic, ok ? "ok" : "failed");
    });

    // Trace all MQTT RX and parse solar power manually as fallback.
    mqtt.onMessage([](char *topic, byte *payload, unsigned int length) {
        String t(topic ? topic : "");
        String p(reinterpret_cast<char *>(payload), length);
        lmg.logTag(LL::Trace, "MQTT", "RX topic=%s payload=%s", t.c_str(), p.c_str());
        if (t.equalsIgnoreCase("tele/tasmota_1DEE45/SENSOR"))
        {
            StaticJsonDocument<512> doc;
            DeserializationError err = deserializeJson(doc, p);
            if (!err)
            {
                int power = doc["ENERGY"]["Power"] | solarPowerW;
                solarPowerW = power;
                lmg.logTag(LL::Debug, "MQTT", "Updated solarPowerW=%d from ENERGY.Power", power);
            }
            else
            {
                lmg.logTag(LL::Warn, "MQTT", "Failed to parse solar JSON: %s", err.c_str());
            }
        }
    });

    // Optional: show receive topics in runtime UI
    // mqtt.addMqttTopicToLiveGroup(ConfigManager, "grid_import_w", "mqtt", "MQTT-Received", "MQTT-Received", 1);

    static bool mqttLogAdded = false;
    if (!mqttLogAdded)
    {
        auto mqttLog = std::make_unique<cm::MQTTLogOutput>(mqtt);
        mqttLog->setLevel(LL::Trace);
        mqttLog->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
        lmg.addOutput(std::move(mqttLog));
        mqttLogAdded = true;
    }
}

static void updateMqttTopics()
{
    String base = mqtt.settings().publishTopicBase.get();
    if (base.isEmpty())
    {
        base = mqtt.getMqttBaseTopic();
    }
    if (base.isEmpty())
    {
        const String cid = mqtt.settings().clientId.get();
        if (!cid.isEmpty()) {
            base = cid;
        }
    }
    if (base.isEmpty())
    {
        const char* hn = WiFi.getHostname();
        if (hn && hn[0]) {
            base = hn;
        }
    }
    if (base.isEmpty())
    {
        base = APP_NAME;
    }

    mqttBaseTopic = base;
    topicPublishSetValueW = mqttBaseTopic + "/SetValue";
    topicPublishGridImportW = mqttBaseTopic + "/GetValue";
    topicPublishTempC = mqttBaseTopic + "/Temperature";
    topicPublishHumidityPct = mqttBaseTopic + "/Humidity";
    topicPublishDewpointC = mqttBaseTopic + "/Dewpoint";
}

static void setFanRelay(bool on)
{
    if (!fanSettings.enabled->get())
    {
        on = false;
    }
    ioManager.setState(IO_FAN_ID, on);
}

static void setHeaterRelay(bool on)
{
    if (!heaterSettings.enabled->get() && !manualOverrideActive)
    {
        on = false;
    }
    ioManager.setState(IO_HEATER_ID, on);
}

static void setManualOverride(bool on)
{
    manualOverrideActive = on;
    if (!manualOverrideActive)
    {
        CheckVentilator(temperature);
        EvaluateHeater(temperature);
    }
}

static bool getFanRelay()
{
    return ioManager.getState(IO_FAN_ID);
}

static bool getHeaterRelay()
{
    return ioManager.getState(IO_HEATER_ID);
}


//----------------------------------------
// MQTT FUNCTIONS
//----------------------------------------

static void publishMqttNow()
{
    if (!mqtt.isConnected())
    {
        return;
    }

    updateMqttTopics();

    mqtt.publishExtraTopic("setvalue_w", topicPublishSetValueW.c_str(), String(inverterSetValue), false);
    mqtt.publishExtraTopic("grid_import_w", topicPublishGridImportW.c_str(), String(currentGridImportW), false);
    mqtt.publishExtraTopic("temperature_c", topicPublishTempC.c_str(), String(temperature), false);
    mqtt.publishExtraTopic("humidity_pct", topicPublishHumidityPct.c_str(), String(Humidity), false);
    mqtt.publishExtraTopic("dewpoint_c", topicPublishDewpointC.c_str(), String(Dewpoint), false);
}

static void publishMqttNowIfNeeded()
{
    const float intervalSec = mqttSettings.publishIntervalSec.get();
    if (intervalSec <= 0.0f)
    {
        return;
    }

    const unsigned long intervalMs = static_cast<unsigned long>(intervalSec * 1000.0f);
    if (intervalMs == 0)
    {
        return;
    }

    const unsigned long now = millis();
    if ((now - lastMqttPublishMs) >= intervalMs)
    {
        lastMqttPublishMs = now;
        publishMqttNow();
    }
}

//----------------------------------------
// HELPER FUNCTIONS
//----------------------------------------

void cb_RS485Listener()
{
  // inverterSetValue = powerSmoother.smooth(currentGridImportW);
  inverterSetValue = powerSmoother->smooth(currentGridImportW);
  // (legacy) previously: if (generalSettings.enableController.get())
  if (limiterSettings.enableController->get())
  {
    // rs485.sendToRS485(static_cast<uint16_t>(inverterSetValue));
  // legacy comment: powerSmoother.setCorrectionOffset(generalSettings.inputCorrectionOffset.get());
  powerSmoother->setCorrectionOffset(limiterSettings.inputCorrectionOffset->get()); // apply the correction offset to the smoother, if needed
    sendToRS485(static_cast<uint16_t>(inverterSetValue));
    lmg.logTag(LL::Debug, "RS485", "Controller enabled -> set inverter to %d W", inverterSetValue);
  }
  else
  {
    lmg.logTag(LL::Info, "RS485", "Controller disabled -> using MAX output");
    sendToRS485(limiterSettings.maxOutput->get()); // send the maxOutput to the RS485 module
  }
}

void testRS232()
{
  // test the RS232 connection
  lmg.logTag(LL::Info, "RS485", "Testing RS232 connection... shorting RX and TX pins");
  lmg.logTag(LL::Info, "RS485", "Baudrate: %d", rs485settings.baudRate->get());
  lmg.logTag(LL::Info, "RS485", "RX Pin: %d", rs485settings.rxPin->get());
  lmg.logTag(LL::Info, "RS485", "TX Pin: %d", rs485settings.txPin->get());
  lmg.logTag(LL::Info, "RS485", "DE Pin: %d", rs485settings.dePin->get());

  Serial2.begin(rs485settings.baudRate->get(), SERIAL_8N1, rs485settings.rxPin->get(), rs485settings.txPin->get());
  Serial2.println("Hello RS485");
  delay(300);
  if (Serial2.available())
  {
    lmg.logTag(LL::Debug, "RS485", "[MAIN] Received on Serial2");
  }
}

void SetupStartDisplay()
{
  Wire.begin(i2cSettings.sdaPin->get(), i2cSettings.sclPin->get());
  Wire.setClock(i2cSettings.busFreq->get());

  const uint8_t address = static_cast<uint8_t>(i2cSettings.displayAddr->get());
  if (!display.begin(SSD1306_SWITCHCAPVCC, address))
  {
    displayInitialized = false;
    displayActive = false;
    lmg.logTag(LL::Warn, "Display", "SSD1306 init failed (addr=0x%02X)", static_cast<unsigned int>(address));
    return;
  }

  displayInitialized = true;
  displayActive = true;

  display.clearDisplay();
  display.drawRect(0, 0, 128, 25, WHITE);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10, 5);
  display.println("Starting!");
  display.display();
}

void SetupStartTemperatureMeasuring()
{
  // init BME280 for temperature and humidity sensor
  bme280.setAddress(BME280_ADDRESS, i2cSettings.sdaPin->get(), i2cSettings.sclPin->get());
  bool isStatus = bme280.begin(
      bme280.BME280_STANDBY_0_5,
      bme280.BME280_FILTER_16,
      bme280.BME280_SPI3_DISABLE,
      bme280.BME280_OVERSAMPLING_2,
      bme280.BME280_OVERSAMPLING_16,
      bme280.BME280_OVERSAMPLING_1,
      bme280.BME280_MODE_NORMAL);
  if (!isStatus) {
    lmg.logTag(LL::Error, "BME280", "BME280 init failed");
  }
  else {
    lmg.logTag(LL::Info, "BME280", "BME280 ready. Starting measurement ticker...");

    temperatureTicker.attach(tempSettings.readIntervalSec->get(), readBme280); // Attach the ticker to read BME280
    readBme280(); // initial read
  }
}

void onWiFiConnected()
{
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    logNetworkIpInfo("onWiFiConnected");
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
    logNetworkIpInfo("onWiFiAPMode");
    lmg.logTag(LL::Info, "WiFi", "AP Mode: http://%s", WiFi.softAPIP().toString().c_str());
}

void readBme280()
{
  // todo: add settings for correcting the values!!!
  //   set sea-level pressure
  bme280.setSeaLevelPressure(tempSettings.seaLevelPressure->get());

  bme280.read();

  temperature = bme280.data.temperature + tempSettings.tempCorrection->get(); // apply correction
  Humidity = bme280.data.humidity + tempSettings.humidityCorrection->get();   // apply correction
  Pressure = bme280.data.pressure;
  Dewpoint = cm::helpers::computeDewPoint(temperature, Humidity);

  // output formatted values to serial console
  lmg.logTag(LL::Trace, "BME280", "-----------------------");
  lmg.logTag(LL::Trace, "BME280", "Temperature: %.1f C", temperature);
  lmg.logTag(LL::Trace, "BME280", "Humidity   : %.1f %%", Humidity);
  lmg.logTag(LL::Trace, "BME280", "Dewpoint   : %.1f C", Dewpoint);
  lmg.logTag(LL::Trace, "BME280", "Pressure   : %.0f hPa", Pressure);
  lmg.logTag(LL::Trace, "BME280", "Altitude   : %.2f m", bme280.data.altitude);
  lmg.logTag(LL::Trace, "BME280", "-----------------------");
}

// Limiter provider moved into setup() for clarity

void WriteToDisplay()
{
  if (!displayInitialized || !displayActive)
  {
    return; // exit the function if the display is not active
  }

  // display.clearDisplay();
  display.fillRect(0, 0, 128, 24, BLACK); // Clear the previous message area
  display.drawRect(0, 0, 128, 24, WHITE);

  display.setTextSize(1);
  display.setTextColor(WHITE);

  // When running in AP mode, show connection info prominently.
  if (WiFi.getMode() == WIFI_AP && WiFi.status() != WL_CONNECTED)
  {
    const IPAddress apIp = WiFi.softAPIP();
    const String apSsid = WiFi.softAPSSID();

    display.setCursor(3, 3);
    display.printf("AP: %s", apIp.toString().c_str());
    display.setCursor(3, 13);
    display.printf("SSID: %s", apSsid.c_str());
    display.display();
    return;
  }

  display.setCursor(3, 3);
  if (temperature > 0)
  {
    display.printf("<- %d W|Temp: %2.1f", currentGridImportW, temperature);
  }
  else
  {
    display.printf("<- %d W", currentGridImportW);
  }

  display.setCursor(3, 13);
  if (Dewpoint != 0)
  {
    display.printf("-> %d W|DP-T: %2.1f", inverterSetValue, Dewpoint);
  }
  else
  {
    display.printf("-> %d W", inverterSetValue);
  }

  display.display();
}

void CheckVentilator(float currentTemperature)
{
  if (manualOverrideActive)
  {
    return;
  }
  if (!fanSettings.enabled->get()) {
    setFanRelay(false);
    return;
  }
  if (currentTemperature >= fanSettings.onThreshold->get()) {
    setFanRelay(true);
  } else if (currentTemperature <= fanSettings.offThreshold->get()) {
    setFanRelay(false);
  }
}

void EvaluateHeater(float currentTemperature){

  if (manualOverrideActive)
  {
    return;
  }

  if(dewpointRiskActive){
    heaterLatchedState = true;
  }

  if(heaterSettings.enabled->get()){
    // Priority 4: Threshold hysteresis (normal mode)
    float onTh = heaterSettings.onTemp->get();
    float offTh = heaterSettings.offTemp->get();
    if(offTh <= onTh) offTh = onTh + 0.3f; // enforce separation

    if(currentTemperature < onTh){
      heaterLatchedState = true;
    }
    if(currentTemperature > offTh){
      heaterLatchedState = false;
    }

  }
  setHeaterRelay(heaterLatchedState);
}

void ShowDisplayOn()
{
  if (!displayInitialized) return;
  displayTicker.detach(); // Stop the ticker to prevent multiple calls
  display.ssd1306_command(SSD1306_DISPLAYON); // Turn on the display
  displayTicker.attach(displaySettings.onTimeSec->get(), ShowDisplayOff); // Reattach the ticker to turn off the display after the specified time
  displayActive = true;
}

void ShowDisplayOff()
{
  if (!displayInitialized) return;
  displayTicker.detach(); // Stop the ticker to prevent multiple calls
  display.ssd1306_command(SSD1306_DISPLAYOFF); // Turn off the display
  // display.fillRect(0, 0, 128, 24, BLACK); // Clear the previous message area

  if (displaySettings.turnDisplayOff->get()){
    displayActive = false;
  }
}
