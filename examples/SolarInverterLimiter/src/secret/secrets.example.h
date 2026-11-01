#pragma once

// Optional: copy to secrets.h, adjust locally, and keep that file out of Git.
#define APP_NAME "SolarInverterLimiter-Demo"

#define MY_WIFI_SSID "YOUR_WIFI_SSID"
#define MY_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define MY_USE_DHCP true

// Used only if DHCP is disabled. Replace these documentation addresses locally.
#define MY_WIFI_IP "192.0.2.130"
#define MY_GATEWAY_IP "192.0.2.1"
#define MY_SUBNET_MASK "255.255.255.0"
#define MY_DNS_IP "192.0.2.1"

#define MY_MQTT_BROKER_IP "mqtt.example.invalid"
#define MY_MQTT_BROKER_PORT 1883
#define MY_MQTT_USERNAME "YOUR_MQTT_USERNAME"
#define MY_MQTT_PASSWORD "YOUR_MQTT_PASSWORD"
#define MY_MQTT_ROOT "SolarLimiter"

#define SETTINGS_PASSWORD ""
#define OTA_PASSWORD ""

// Optional roaming preference: define WIFI_FILTER_MAC_PRIORITY locally if needed.
