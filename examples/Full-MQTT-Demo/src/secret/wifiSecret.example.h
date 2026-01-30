#pragma once

// Copy this file to wifiSecret.h and adjust values.
// NOTE: wifiSecret.h must stay out of version control.

// WiFi credentials
#define MY_WIFI_SSID "YOUR_WIFI_SSID"
#define MY_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Optional static IP (only used when DHCP is disabled in settings)
#define MY_WIFI_IP "192.168.2.126"
#define MY_GATEWAY_IP "192.168.2.250"
#define MY_SUBNET_MASK "255.255.255.0"
#define MY_DNS_IP "192.168.2.250"
#define MY_USE_DHCP true

#define MY_MQTT_BROKER_IP "192.168.2.3"
#define MY_MQTT_BROKER_PORT 1883
#define MY_MQTT_USERNAME "mqtt-username"
#define MY_MQTT_PASSWORD "mqtt-password"
#define MY_MQTT_ROOT "TestDevice01"