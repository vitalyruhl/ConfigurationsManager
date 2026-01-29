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
    if (!logger && !loggerVerbose) {
        return;
    }
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    const char* msg = buffer;
    bool useVerbose = false;
    if (msg[0] == '[') {
        const char* end = strchr(msg, ']');
        if (end && end > msg + 1) {
            const size_t tokenLen = static_cast<size_t>(end - msg - 1);
            auto tokenEquals = [&](const char* value) {
                const size_t valueLen = strlen(value);
                return valueLen == tokenLen && strncmp(msg + 1, value, tokenLen) == 0;
            };

            const bool isVerboseToken = tokenEquals("DEBUG") || tokenEquals("D") || tokenEquals("TRACE") || tokenEquals("T") ||
                                        tokenEquals("VERBOSE") || tokenEquals("V");
            const bool isLevelToken = isVerboseToken || tokenEquals("INFO") || tokenEquals("I") || tokenEquals("WARN") || tokenEquals("W") ||
                                      tokenEquals("ERROR") || tokenEquals("E") || tokenEquals("FATAL") || tokenEquals("F");

            if (isVerboseToken) {
                useVerbose = true;
            }
            if (isLevelToken) {
                msg = end + 1;
                if (*msg == ' ') {
                    msg++;
                }
            }
        }
    }

    if (useVerbose && loggerVerbose) {
        loggerVerbose(msg);
    } else if (logger) {
        logger(msg);
    } else if (loggerVerbose) {
        loggerVerbose(msg);
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
