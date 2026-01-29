#include "LoggingManager.h"

namespace cm {

static const char* levelToString(LoggingManager::Level level)
{
    switch (level) {
    case LoggingManager::Level::Fatal:
        return "FATAL";
    case LoggingManager::Level::Error:
        return "ERROR";
    case LoggingManager::Level::Warn:
        return "WARN";
    case LoggingManager::Level::Info:
        return "INFO";
    case LoggingManager::Level::Debug:
        return "DEBUG";
    case LoggingManager::Level::Trace:
        return "TRACE";
    default:
        return "OFF";
    }
}

void LoggingManager::SerialOutput::log(Level level, const char* tag, const char* message, unsigned long timestampMs)
{
    if (level == Level::Off || level > getLevel()) {
        return;
    }
    if (!shouldLog(level, tag, message)) {
        return;
    }
    if (!allowRate(timestampMs)) {
        return;
    }

    const String& prefix = getPrefix();
    const bool compact = (getFormat() == Output::Format::Compact);
    switch (getTimestampMode()) {
    case Output::TimestampMode::Millis:
        serial_.print('[');
        serial_.print(timestampMs);
        serial_.print("] ");
        break;
    case Output::TimestampMode::DateTime: {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0)) {
            char buf[32];
            const String& fmt = getTimestampFormat();
            const char* formatStr = fmt.length() ? fmt.c_str() : "%Y-%m-%d %H:%M:%S";
            strftime(buf, sizeof(buf), formatStr, &timeinfo);
            serial_.print('[');
            serial_.print(buf);
            serial_.print("] ");
        } else {
            // NTP not synced yet: fall back to millis so we still show a timestamp
            serial_.print('[');
            serial_.print(timestampMs);
            serial_.print("] ");
        }
        break;
    }
    default:
        break;
    }
    if (!compact) {
        serial_.print('[');
        serial_.print(levelToString(level));
        serial_.print("] ");
        if (tag && tag[0]) {
            serial_.print('[');
            serial_.print(tag);
            serial_.print("] ");
        }
    }
    if (prefix.length()) {
        serial_.print(prefix);
    }
    serial_.println(message ? message : "");
}

#if !CM_DISABLE_GUI_LOGGING
LoggingManager::GuiOutput::GuiOutput(ConfigManagerClass& configManager, size_t startupBufferSize)
    : configManager_(configManager)
    , bufferLimit_(startupBufferSize)
{
    configManager_.setGuiLoggingEnabled(true);
#if CM_ENABLE_WS_PUSH
    const String readyPayload = makeReadyPayload_();
    configManager_.addWebSocketConnectListener([this](AsyncWebSocketClient* client) {
        if (!client) {
            return;
        }
        configManager_.sendWebSocketText(client, makeReadyPayload_());
        flushToClient_(client);
        bufferEnabled_ = false;
    });
    if (configManager_.getWebSocketClientCount() > 0) {
        configManager_.sendWebSocketText(readyPayload);
        bufferEnabled_ = false;
    }
#endif
}

void LoggingManager::GuiOutput::log(Level level, const char* tag, const char* message, unsigned long timestampMs)
{
    if (level == Level::Off || level > getLevel()) {
        return;
    }
    if (!shouldLog(level, tag, message)) {
        return;
    }
    const String payload = makePayload_(level, tag, message, timestampMs);
#if CM_ENABLE_WS_PUSH
    if (configManager_.getWebSocketClientCount() > 0) {
        enqueuePending_(payload);
        return;
    }
#endif
    if (bufferEnabled_ && bufferLimit_ > 0) {
        enqueue_(payload);
    }
}

void LoggingManager::GuiOutput::tick(unsigned long nowMs)
{
    (void)nowMs;
#if CM_ENABLE_WS_PUSH
    if (configManager_.getWebSocketClientCount() == 0 || pending_.empty()) {
        return;
    }
    size_t sent = 0;
    while (!pending_.empty() && sent < maxPerTick_) {
        configManager_.sendWebSocketText(pending_.front());
        pending_.erase(pending_.begin());
        sent++;
    }
#endif
}

void LoggingManager::GuiOutput::enqueue_(const String& payload)
{
    if (bufferLimit_ == 0) {
        return;
    }
    if (buffer_.size() >= bufferLimit_) {
        buffer_.erase(buffer_.begin());
    }
    buffer_.push_back(payload);
}

void LoggingManager::GuiOutput::enqueuePending_(const String& payload)
{
    if (pendingLimit_ == 0) {
        return;
    }
    if (pending_.size() >= pendingLimit_) {
        pending_.erase(pending_.begin());
    }
    pending_.push_back(payload);
}

String LoggingManager::GuiOutput::makeReadyPayload_() const
{
    return String("{\"type\":\"logReady\"}");
}

void LoggingManager::GuiOutput::flushToClient_(AsyncWebSocketClient* client)
{
#if CM_ENABLE_WS_PUSH
    if (!client || buffer_.empty()) {
        return;
    }
    for (const auto& entry : buffer_) {
        enqueuePending_(entry);
    }
    buffer_.clear();
#else
    (void)client;
    buffer_.clear();
#endif
}

String LoggingManager::GuiOutput::escapeJson_(const char* value)
{
    if (!value) {
        return String();
    }
    String out;
    const char* p = value;
    while (*p) {
        const char c = *p++;
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

String LoggingManager::GuiOutput::makePayload_(Level level, const char* tag, const char* message, unsigned long timestampMs) const
{
    String payload;
    payload.reserve(128);
    payload += "{\"type\":\"log\",\"ts\":";
    const auto tsMode = getTimestampMode();
    if (tsMode == Output::TimestampMode::None) {
        payload += "null";
    } else {
        payload += String(timestampMs);
    }
    if (tsMode == Output::TimestampMode::DateTime) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0)) {
            char buf[32];
            const String& fmt = getTimestampFormat();
            const char* formatStr = fmt.length() ? fmt.c_str() : "%Y-%m-%d %H:%M:%S";
            strftime(buf, sizeof(buf), formatStr, &timeinfo);
            payload += ",\"dt\":\"";
            payload += escapeJson_(buf);
            payload += "\"";
        } else {
            payload += ",\"dt\":\"";
            payload += String(timestampMs);
            payload += "\"";
        }
    } else if (tsMode == Output::TimestampMode::Millis) {
        payload += ",\"dt\":\"";
        payload += String(timestampMs);
        payload += "\"";
    }
    payload += ",\"level\":\"";
    payload += levelToString(level);
    payload += "\"";
    if (tag && tag[0]) {
        payload += ",\"tag\":\"";
        payload += escapeJson_(tag);
        payload += "\"";
    }
    const String prefix = getPrefix();
    payload += ",\"msg\":\"";
    if (prefix.length()) {
        payload += escapeJson_(prefix.c_str());
    }
    payload += escapeJson_(message ? message : "");
    payload += "\"}";
    return payload;
}
#endif

void LoggingManager::addOutput(std::unique_ptr<Output> output)
{
    if (!output) {
        return;
    }
    outputs_.push_back(std::move(output));
}

void LoggingManager::clearOutputs()
{
    outputs_.clear();
}

void LoggingManager::setTag(const char* tag)
{
    baseTag_ = tag ? String(tag) : String();
}

void LoggingManager::clearTag()
{
    baseTag_.clear();
}

void LoggingManager::pushTag(const char* tag)
{
    if (tag && tag[0]) {
        tagStack_.push_back(String(tag));
    }
}

void LoggingManager::popTag()
{
    if (!tagStack_.empty()) {
        tagStack_.pop_back();
    }
}

void LoggingManager::logTag(Level level, const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    logV(level, tag, format, args);
    va_end(args);
}

void LoggingManager::log(Level level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    logV(level, nullptr, format, args);
    va_end(args);
}

void LoggingManager::log(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    logV(globalLevel_, nullptr, format, args);
    va_end(args);
}

void LoggingManager::logV(Level level, const char* tag, const char* format, va_list args)
{
    if (!shouldLog_(level)) {
        return;
    }
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format ? format : "", args);

    const unsigned long ts = millis();
    const char* msg = buffer;
    String extractedTag;
    if (!tag && msg && msg[0] == '[') {
        const char* end = strchr(msg, ']');
        if (end && end > msg + 1) {
            const size_t tokenLen = static_cast<size_t>(end - msg - 1);
            auto tokenEquals = [&](const char* value) {
                const size_t valueLen = strlen(value);
                return valueLen == tokenLen && strncmp(msg + 1, value, tokenLen) == 0;
            };
            const bool isLevelToken = tokenEquals("DEBUG") || tokenEquals("D") || tokenEquals("TRACE") || tokenEquals("T") ||
                                      tokenEquals("VERBOSE") || tokenEquals("V") || tokenEquals("INFO") || tokenEquals("I") ||
                                      tokenEquals("WARN") || tokenEquals("W") || tokenEquals("ERROR") || tokenEquals("E") ||
                                      tokenEquals("FATAL") || tokenEquals("F");
            if (!isLevelToken) {
                extractedTag = String(msg + 1).substring(0, tokenLen);
                msg = end + 1;
                if (*msg == ' ') {
                    msg++;
                }
            }
        }
    }
    const String effectiveTag = buildTag_((tag && tag[0]) ? tag : (extractedTag.length() ? extractedTag.c_str() : nullptr));
    // Strip leading level tokens like "[I]" or "[INFO]" if present in the message
    if (msg) {
        while (*msg == ' ') {
            msg++;
        }
    }
    if (msg && msg[0] == '[') {
        const char* end = strchr(msg, ']');
        if (end && end > msg + 1) {
            const size_t tokenLen = static_cast<size_t>(end - msg - 1);
            auto tokenEquals = [&](const char* value) {
                const size_t valueLen = strlen(value);
                return valueLen == tokenLen && strncmp(msg + 1, value, tokenLen) == 0;
            };
            const bool isLevelToken = tokenEquals("DEBUG") || tokenEquals("D") || tokenEquals("TRACE") || tokenEquals("T") ||
                                      tokenEquals("VERBOSE") || tokenEquals("V") || tokenEquals("INFO") || tokenEquals("I") ||
                                      tokenEquals("WARN") || tokenEquals("W") || tokenEquals("ERROR") || tokenEquals("E") ||
                                      tokenEquals("FATAL") || tokenEquals("F");
            if (isLevelToken) {
                msg = end + 1;
                if (*msg == ' ') {
                    msg++;
                }
            }
        }
    }
    for (auto& output : outputs_) {
        if (output) {
            output->log(level, effectiveTag.length() ? effectiveTag.c_str() : nullptr, msg, ts);
        }
    }
}

void LoggingManager::loop()
{
    const unsigned long now = millis();
    for (auto& output : outputs_) {
        if (output) {
            output->tick(now);
        }
    }
}

void LoggingManager::attachToConfigManager(Level level, Level verboseLevel, const char* tag)
{
    defaultLevel_ = level;
    if (tag == nullptr) {
        defaultTag_ = "ConfigManager";
    } else if (tag[0] == '\0') {
        defaultTag_ = "";
    } else {
        defaultTag_ = tag;
    }
    verboseLevel_ = verboseLevel;
    verboseTag_ = defaultTag_;

    ConfigManagerClass::setLogger([this](const char* msg) {
        this->logTag(defaultLevel_, defaultTag_, "%s", msg ? msg : "");
    });
    ConfigManagerClass::setLoggerVerbose([this](const char* msg) {
        this->logTag(verboseLevel_, verboseTag_, "%s", msg ? msg : "");
    });
}

bool LoggingManager::shouldLog_(Level level) const
{
    if (level == Level::Off || globalLevel_ == Level::Off) {
        return false;
    }
    return level <= globalLevel_;
}

String LoggingManager::buildTag_(const char* tag) const
{
    String combined;
    auto appendPart = [&combined](const String& part) {
        if (part.length() == 0) {
            return;
        }
        if (combined.length() > 0) {
            combined += "/";
        }
        combined += part;
    };
    if (baseTag_.length()) {
        appendPart(baseTag_);
    }
    if (tag && tag[0]) {
        appendPart(String(tag));
    }
    for (const auto& extra : tagStack_) {
        appendPart(extra);
    }
    return combined;
}

} // namespace cm
