#include "ConfigManager.h"

// Global instances
AsyncWebServer server(80);
ConfigManagerClass ConfigManager;

// Static logger instances
std::function<void(const char*)> ConfigManagerClass::logger = nullptr;
std::function<void(const char*)> ConfigManagerClass::loggerVerbose = nullptr;

#if CM_ENABLE_LOGGING
void ConfigManagerClass::setLogger(LogCallback cb) {
    logger = cb;
    if (!loggerVerbose) {
        loggerVerbose = cb;
    }
}

void ConfigManagerClass::setLoggerVerbose(LogCallback cb) {
    loggerVerbose = cb;
}

void ConfigManagerClass::log_message(const char *format, ...)
{
    if (logger) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        logger(buffer);
    }
}

void ConfigManagerClass::log_verbose_message(const char *format, ...)
{
    if (loggerVerbose) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        loggerVerbose(buffer);
    }
}
#endif
