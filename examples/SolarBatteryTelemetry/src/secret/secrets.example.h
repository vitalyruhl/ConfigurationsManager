#pragma once

// Optional: copy to secrets.h and set your own values; never commit secrets.h.
// With this template alone, use CM's setup AP to configure the application.
#define APP_NAME "SolarBatteryTelemetry"

#define MY_WIFI_SSID ""
#define MY_WIFI_PASSWORD ""
#define MY_USE_DHCP true
// Used only if DHCP is disabled; configure these for your own network first.
#define MY_WIFI_IP ""
#define MY_GATEWAY_IP ""
#define MY_SUBNET_MASK ""
#define MY_DNS_IP ""

// MQTT is disabled when secrets.h is absent. Configure the broker in Settings.
#define MY_MQTT_BROKER_IP ""
#define MY_MQTT_BROKER_PORT 1883
#define MY_MQTT_USERNAME ""
#define MY_MQTT_PASSWORD ""
#define MY_MQTT_ROOT "SolarBatteryTelemetry"

// Set your own Settings and OTA passwords before commissioning.
#define SETTINGS_PASSWORD ""
#define OTA_PASSWORD ""
#define WIFI_FILTER_MAC_PRIORITY ""

// Optional Victron Instant Readout MAC and 16-byte key (32 hexadecimal digits).
// This key is different from the VictronConnect pairing PIN.
#define VICTRON_READOUT_MAC ""
#define VICTRON_READOUT_KEY ""

// Optional JBD Bluetooth MAC. An empty value leaves the source unconfigured.
#define BMS_READOUT_MAC ""
