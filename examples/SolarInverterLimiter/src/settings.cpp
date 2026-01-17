
#include "settings.h"

#include <cstdarg>
#include <cstdio>
#include <functional>

// The ConfigManager v3.x library declares these symbols in headers and uses them from
// multiple translation units, but does not provide a compiled definition.
// Provide the required storage and logging helpers here.

ConfigManagerClass ConfigManager;
std::function<void(const char*)> ConfigManagerClass_logger;

#if CM_ENABLE_LOGGING
ConfigManagerClass::LogCallback ConfigManagerClass::logger;

void ConfigManagerClass::setLogger(LogCallback cb)
{
	logger = cb;
	ConfigManagerClass_logger = cb;
}

void ConfigManagerClass::log_message(const char* format, ...)
{
	if (!ConfigManagerClass_logger) {
		return;
	}

	char buffer[256];

	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	ConfigManagerClass_logger(buffer);
}
#endif

ConfigManagerClass &cfg = ConfigManager;
WiFi_Settings wifiSettings;
MQTT_Settings mqttSettings;
LimiterSettings limiterSettings;
TempSettings tempSettings;
I2CSettings i2cSettings;
FanSettings fanSettings;
HeaterSettings heaterSettings;
DisplaySettings displaySettings;
SystemSettings systemSettings;
ButtonSettings buttonSettings;
RS485_Settings rs485settings;
NTPSettings ntpSettings; // ntpSettings
SigmaLogLevel logLevel = SIGMALOG_WARN; // SIGMALOG_OFF = 0, SIGMALOG_INTERNAL, SIGMALOG_FATAL, SIGMALOG_ERROR, SIGMALOG_WARN, SIGMALOG_INFO, SIGMALOG_DEBUG, SIGMALOG_ALL
