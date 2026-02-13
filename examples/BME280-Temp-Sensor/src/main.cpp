#include <Arduino.h>

#include <BME280_I2C.h>
#include <Ticker.h>
#include <WiFi.h>

#include "ConfigManager.h"

#include "alarm/AlarmManager.h"
#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "helpers/HelperModule.h"

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define CM_HAS_WIFI_SECRETS 1
#else
#define CM_HAS_WIFI_SECRETS 0
#endif

#ifndef BME280_ADDRESS
#define BME280_ADDRESS 0x76
#endif

#define VERSION CONFIGMANAGER_VERSION
#ifndef APP_NAME
#define APP_NAME "CM-BME280-Temp-Sensor"
#endif

#ifndef SETTINGS_PASSWORD
#define SETTINGS_PASSWORD ""
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD SETTINGS_PASSWORD
#endif

// I2C pins for the BME280 sensor
#define I2C_SDA 21
#define I2C_SCL 22

extern ConfigManagerClass ConfigManager;

// Built-in core settings templates.
static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;
static cm::AlarmManager alarmManager;

static BME280_I2C bme280;
static Ticker temperatureTicker;

static float temperature = 0.0f;
static float dewPoint = 0.0f;
static float humidity = 0.0f;
static float pressure = 0.0f;

static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.myCSSTempClass { color:rgb(198, 16, 16) !important; font-weight:900!important; font-size: 1.2rem!important; }
)CSS";


// Global WiFi event hooks used by ConfigManager.
// These are invoked internally by ConfigManager's WiFi manager on state transitions.
// If you don't provide them, the library provides weak no-op defaults (see src/default_hooks.cpp).
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
static void setupNetworkDefaults();

struct TempSettings
{
    Config<float> *tempCorrection = nullptr;
    Config<float> *humidityCorrection = nullptr;
    Config<int> *seaLevelPressure = nullptr;
    Config<int> *readIntervalSec = nullptr;

    void create()
    {
        tempCorrection = &ConfigManager.addSettingFloat("TCO")
                              .name("Temperature Correction")
                              .category("Temp")
                              .defaultValue(0.0f)
                              .build();
        humidityCorrection = &ConfigManager.addSettingFloat("HYO")
                                 .name("Humidity Correction")
                                 .category("Temp")
                                 .defaultValue(0.0f)
                                 .build();
        seaLevelPressure = &ConfigManager.addSettingInt("SLP")
                                .name("Sea Level Pressure")
                                .category("Temp")
                                .defaultValue(1013)
                                .build();
        readIntervalSec = &ConfigManager.addSettingInt("ReadTemp")
                               .name("Read Temp/Humidity every (s)")
                               .category("Temp")
                               .defaultValue(30)
                               .build();
    }

    void placeInUi() const
    {
        if (!tempCorrection || !humidityCorrection || !seaLevelPressure || !readIntervalSec)
        {
            return;
        }
        ConfigManager.addSettingsPage("Temp", 40);
        ConfigManager.addSettingsGroup("Temp", "Temp", "Temperature", 40);
        ConfigManager.addToSettingsGroup(tempCorrection->getKey(), "Temp", "Temp", "Temperature", 10);
        ConfigManager.addToSettingsGroup(humidityCorrection->getKey(), "Temp", "Temp", "Temperature", 20);
        ConfigManager.addToSettingsGroup(seaLevelPressure->getKey(), "Temp", "Temp", "Temperature", 30);
        ConfigManager.addToSettingsGroup(readIntervalSec->getKey(), "Temp", "Temp", "Temperature", 40);
    }
};

static TempSettings tempSettings;

static void setupRuntimeUI()
{
    auto live = ConfigManager.liveGroup("sensors")
                    .page("Sensors", 10)
                    .card("BME280 - Temperature Sensor");

    live.value("temp", []() { return temperature; })
        .label("Temperature")
        .unit("C")
        .precision(1)
        .addCSSClass("myCSSTempClass")
        .order(10);

    live.value("hum", []() { return humidity; })
        .label("Humidity")
        .unit("%")
        .precision(1)
        .order(11);

    live.value("pressure", []() { return pressure; })
        .label("Pressure")
        .unit("hPa")
        .precision(1)
        .order(12);

    auto dewpointGroup = ConfigManager.liveGroup("sensors")
                            .page("Sensors", 10)
                            .card("BME280 - Temperature Sensor")
                            .group("Dewpoint", 20);

    dewpointGroup.value("dew", []() { return dewPoint; })
        .label("Dewpoint")
        .unit("C")
        .precision(1)
        .order(20);

    alarmManager.addDigitalWarning({
        .id = "dewRisk",
        .name = "Condensation Risk",
        .kind = cm::AlarmKind::DigitalActive,
        .severity = cm::AlarmSeverity::Warning,
        .enabled = true,
        .getter = []() { return temperature < dewPoint; },
    });

    alarmManager.addWarningToLive(
        "dewRisk",
        30,
        "Sensors",
        "BME280 - Temperature Sensor",
        "Dewpoint",
        "Condensation Risk");
}

static void readBme280()
{
    bme280.setSeaLevelPressure(tempSettings.seaLevelPressure->get());
    bme280.read();

    temperature = bme280.data.temperature + tempSettings.tempCorrection->get();
    humidity = bme280.data.humidity + tempSettings.humidityCorrection->get();
    pressure = bme280.data.pressure;
    dewPoint = cm::helpers::computeDewPoint(temperature, humidity);
}

static void setupTemperatureMeasuring()
{
    Serial.println("[I] Initializing BME280 sensor...");

    bme280.setAddress(BME280_ADDRESS, I2C_SDA, I2C_SCL);

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
        Serial.println("[E] BME280 not initialized - continuing without temperature sensor");
        return;
    }

    Serial.println("[I] BME280 ready! Starting temperature ticker...");
    int interval = tempSettings.readIntervalSec->get();
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
        Serial.print("[CM] ");
        Serial.println(msg);
    });

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(VERSION);

    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
    ConfigManager.enableBuiltinSystemProvider();
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);


    coreSettings.attachWiFi(ConfigManager);
    coreSettings.attachSystem(ConfigManager);
    coreSettings.attachNtp(ConfigManager);

    tempSettings.create();
    tempSettings.placeInUi();

    ConfigManager.loadAll();
    setupNetworkDefaults();

    setupRuntimeUI();

    setupTemperatureMeasuring();

#if defined(WIFI_FILTER_MAC_PRIORITY)
    ConfigManager.setAccessPointMacPriority(WIFI_FILTER_MAC_PRIORITY);
#endif
    ConfigManager.startWebServer();

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

    if (systemSettings.otaPassword.get() != OTA_PASSWORD)
    {
        systemSettings.otaPassword.save(OTA_PASSWORD);
    }
}

void loop()
{
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();

    alarmManager.update();

    static unsigned long lastLoopLog = 0;
    if (millis() - lastLoopLog > 60000)
    {
        lastLoopLog = millis();
        Serial.printf("[MAIN] Loop running, WiFi status: %d, heap: %d\n", WiFi.status(), ESP.getFreeHeap());
    }
}
