#include "ConfigManager.h"

// Global instances
AsyncWebServer server(80);
ConfigManagerClass ConfigManager;

// Static logger instance
std::function<void(const char*)> ConfigManagerClass::logger = nullptr;

#if CM_ENABLE_LOGGING
void ConfigManagerClass::setLogger(LogCallback cb) {
    logger = cb;
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
#endif
