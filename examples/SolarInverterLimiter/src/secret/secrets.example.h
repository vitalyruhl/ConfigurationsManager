#pragma once

// Copy this file to secrets.h and adjust values.
// NOTE: secrets.h must stay out of version control.

#define APP_NAME "SolarInverterLimiter-Demo"

// WiFi credentials
#define MY_WIFI_SSID "YOUR_WIFI_SSID"
#define MY_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Optional static IP (only used when DHCP is disabled in settings)
#define MY_WIFI_IP "192.168.0.130"
#define MY_GATEWAY_IP "192.168.0.1"
#define MY_SUBNET_MASK "255.255.255.0"
#define MY_DNS_IP "192.168.0.1"
#define MY_USE_DHCP true

#define MY_MQTT_BROKER_IP "192.168.0.10"
#define MY_MQTT_BROKER_PORT 1883
#define MY_MQTT_USERNAME "mqtt-username"
#define MY_MQTT_PASSWORD "mqtt-password"
#define MY_MQTT_ROOT "SolarInverterLimiter-Demo"

#define SETTINGS_PASSWORD ""
#define OTA_PASSWORD ""

// dev-Station
#define WIFI_FILTER_MAC_PRIORITY "60:B5:8D:4C:E1:D5"
