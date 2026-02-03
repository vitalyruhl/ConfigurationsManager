#include <Arduino.h>

#include <BME280_I2C.h>
#include <Ticker.h>
#include <WiFi.h>

#include "ConfigManager.h"

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "helpers/HelperModule.h"

#ifndef BME280_ADDRESS
#define BME280_ADDRESS 0x76
#endif

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-BME280-Temp-Sensor"

// Minimal skeleton: do not hardcode WiFi credentials in code.
// Leave SSID empty to start in AP mode and configure via Web UI.
static const char SETTINGS_PASSWORD[] = "cm";

// I2C pins for the BME280 sensor
#define I2C_SDA 21
#define I2C_SCL 22

static inline ConfigManagerRuntime &CRM() { return ConfigManager.getRuntime(); }

extern ConfigManagerClass ConfigManager;

// Built-in core settings templates.
static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

static BME280_I2C bme280;
static Ticker temperatureTicker;

static float temperature = 0.0f;
static float dewPoint = 0.0f;
static float humidity = 0.0f;
static float pressure = 0.0f;

// Global WiFi event hooks used by ConfigManager.
// These are invoked internally by ConfigManager's WiFi manager on state transitions.
// If you don't provide them, the library provides weak no-op defaults (see src/default_hooks.cpp).
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

struct TempSettings
{
    Config<float> tempCorrection;
    Config<float> humidityCorrection;
    Config<int> seaLevelPressure;
    Config<int> readIntervalSec;

    TempSettings()
        : tempCorrection(ConfigOptions<float>{.key = "TCO", .name = "Temperature Correction", .category = "Temp", .defaultValue = 0.0f}),
          humidityCorrection(ConfigOptions<float>{.key = "HYO", .name = "Humidity Correction", .category = "Temp", .defaultValue = 0.0f}),
          seaLevelPressure(ConfigOptions<int>{.key = "SLP", .name = "Sea Level Pressure", .category = "Temp", .defaultValue = 1013}),
          readIntervalSec(ConfigOptions<int>{.key = "ReadTemp", .name = "Read Temp/Humidity every (s)", .category = "Temp", .defaultValue = 30})
    {
    }

    void create()
    {
        ConfigManager.addSetting(&tempCorrection);
        ConfigManager.addSetting(&humidityCorrection);
        ConfigManager.addSetting(&seaLevelPressure);
        ConfigManager.addSetting(&readIntervalSec);
    }

    void placeInUi() const
    {
        ConfigManager.addSettingsPage("Temp", 40);
        ConfigManager.addSettingsGroup("Temp", "Temp", "Temperature", 40);
        ConfigManager.addToSettingsGroup(tempCorrection.getKey(), "Temp", "Temp", "Temperature", 10);
        ConfigManager.addToSettingsGroup(humidityCorrection.getKey(), "Temp", "Temp", "Temperature", 20);
        ConfigManager.addToSettingsGroup(seaLevelPressure.getKey(), "Temp", "Temp", "Temperature", 30);
        ConfigManager.addToSettingsGroup(readIntervalSec.getKey(), "Temp", "Temp", "Temperature", 40);
    }
};

static TempSettings tempSettings;

static void setupRuntimeUI()
{
    CRM().addRuntimeProvider("sensors", [](JsonObject &data) {
        data["temp"] = temperature;
        data["hum"] = humidity;
        data["dew"] = dewPoint;
        data["pressure"] = pressure;
    });

    RuntimeFieldMeta tempMeta;
    tempMeta.group = "sensors";
    tempMeta.key = "temp";
    tempMeta.label = "Temperature";
    tempMeta.unit = "C";
    tempMeta.precision = 1;
    tempMeta.order = 10;
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
    dewMeta.unit = "C";
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
}

static void readBme280()
{
    bme280.setSeaLevelPressure(tempSettings.seaLevelPressure.get());
    bme280.read();

    temperature = bme280.data.temperature + tempSettings.tempCorrection.get();
    humidity = bme280.data.humidity + tempSettings.humidityCorrection.get();
    pressure = bme280.data.pressure;
    dewPoint = cm::helpers::computeDewPoint(temperature, humidity);
}

static void setupTemperatureMeasuring()
{
    Serial.println("[TEMP] Initializing BME280 sensor...");

    bme280.setAddress(BME280_ADDRESS, I2C_SDA, I2C_SCL);

    Serial.println("[TEMP] Starting BME280.begin()...");
    const bool ok = bme280.begin(
        bme280.BME280_STANDBY_0_5,
        bme280.BME280_FILTER_OFF,
        bme280.BME280_SPI3_DISABLE,
        bme280.BME280_OVERSAMPLING_1,
        bme280.BME280_OVERSAMPLING_1,
        bme280.BME280_OVERSAMPLING_1,
        bme280.BME280_MODE_NORMAL);

    if (!ok)
    {
        Serial.println("[TEMP] BME280 not initialized - continuing without temperature sensor");
        return;
    }

    Serial.println("[TEMP] BME280 ready! Starting temperature ticker...");
    int interval = tempSettings.readIntervalSec.get();
    if (interval < 2)
    {
        interval = 2;
    }
    temperatureTicker.attach(static_cast<float>(interval), readBme280);
    readBme280();
}

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

    // Keep OTA enable flag reactive (optional), even though OTA init happens in wifiServices.onConnected().
    systemSettings.allowOTA.setCallback([](bool enabled) {
        Serial.printf("[MAIN] OTA setting changed to: %s\n", enabled ? "enabled" : "disabled");
        ConfigManager.getOTAManager().enable(enabled);
    });

    tempSettings.create();
    tempSettings.placeInUi();

    ConfigManager.loadAll();

    // Ensure OTAManager state matches the persisted setting.
    ConfigManager.getOTAManager().enable(systemSettings.allowOTA.get());

    ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");// dev-Station
    ConfigManager.startWebServer();
    ConfigManager.getWiFiManager().setAutoRebootTimeout((unsigned long)wifiSettings.rebootTimeoutMin.get());

    ConfigManager.addLivePage("Sensors", 10);
    ConfigManager.addLiveGroup("Sensors", "Sensors", "Sensor Readings", 10);
    setupRuntimeUI();

    ConfigManager.enableWebSocketPush();
    ConfigManager.setWebSocketInterval(1000);
    ConfigManager.setPushOnConnect(true);

    setupTemperatureMeasuring();

    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
    {
        Serial.printf("[MAIN] Web server running at: %s (AP Mode)\n", WiFi.softAPIP().toString().c_str());
    }
    else if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[MAIN] Web server running at: %s (Station Mode)\n", WiFi.localIP().toString().c_str());
    }
    else
    {
        Serial.println("[MAIN] Web server running (IP not available)");
    }

    Serial.println("[MAIN] Setup completed successfully. Starting main loop...");
}

void onWiFiConnected()
{
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    Serial.printf("[INFO] Station Mode: http://%s\n", WiFi.localIP().toString().c_str());
}

void onWiFiDisconnected()
{
    wifiServices.onDisconnected();
    Serial.println("[ERROR] WiFi disconnected");
}

void onWiFiAPMode()
{
    wifiServices.onAPMode();
    Serial.printf("[INFO] AP Mode: http://%s\n", WiFi.softAPIP().toString().c_str());
}

void loop()
{
    ConfigManager.updateLoopTiming();
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();
    ConfigManager.handleWebsocketPush();
    ConfigManager.handleOTA();
    ConfigManager.handleRuntimeAlarms();

    static unsigned long lastLoopLog = 0;
    if (millis() - lastLoopLog > 60000)
    {
        lastLoopLog = millis();
        Serial.printf("[MAIN] Loop running, WiFi status: %d, heap: %d\n", WiFi.status(), ESP.getFreeHeap());
    }
}
