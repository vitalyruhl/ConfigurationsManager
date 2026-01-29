#pragma once

#include "MQTTManager.h"
#include "../logging/LoggingManager.h"
#include <cstring>

namespace cm {

class MQTTLogOutput : public LoggingManager::Output {
public:
    using Level = LoggingManager::Level;
    enum RetainedMask : uint8_t {
        RetainInfo = 1 << 0,
        RetainWarn = 1 << 1,
        RetainError = 1 << 2,
    };

    explicit MQTTLogOutput(MQTTManager& mqtt, const char* logRoot = "log")
        : mqtt_(mqtt)
        , logRoot_(logRoot && logRoot[0] ? String(logRoot) : String("log"))
    {
    }

    void addTimestamp(TimestampMode mode) { setTimestampMode(mode); }
    void setRateLimitMs(uint32_t ms) { setMinIntervalMs(ms); }

    void setLogRoot(const char* logRoot)
    {
        logRoot_ = (logRoot && logRoot[0]) ? String(logRoot) : String("log");
    }

    void setUnretainedEnabled(bool enabled) { unretainedEnabled_ = enabled; }

    void setRetainedLevels(bool info, bool warn, bool error)
    {
        retainedMask_ = 0;
        if (info) retainedMask_ |= RetainInfo;
        if (warn) retainedMask_ |= RetainWarn;
        if (error) retainedMask_ |= RetainError;
    }

    void setCustomTagPrefix(const char* prefix)
    {
        customTagPrefix_ = (prefix && prefix[0]) ? String(prefix) : String();
    }

    void setCustomRetainedEnabled(bool enabled) { customRetainedEnabled_ = enabled; }

    void log(Level level, const char* tag, const char* message, unsigned long timestampMs) override
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
        if (!mqtt_.isConnected()) {
            return;
        }
        const String base = mqtt_.getMqttBaseTopic();
        if (base.isEmpty()) {
            return;
        }

        const String payload = buildPayload_(level, tag, message, timestampMs);
        const String root = base + "/" + logRoot_;

        if (unretainedEnabled_) {
            const String topic = root + "/" + levelToString_(level) + "/LogMessages";
            mqtt_.publishRaw(topic.c_str(), payload.c_str(), false);
        }

        if (shouldRetainLevel_(level)) {
            const String topic = root + "/last/" + levelToString_(level);
            mqtt_.publishRaw(topic.c_str(), payload.c_str(), true);
        }

        if (customRetainedEnabled_ && isCustomTag_(tag)) {
            const String topic = root + "/last/Custom";
            mqtt_.publishRaw(topic.c_str(), payload.c_str(), true);
        }
    }

private:
    MQTTManager& mqtt_;
    String logRoot_;
    bool unretainedEnabled_ = true;
    uint8_t retainedMask_ = RetainInfo | RetainWarn | RetainError;
    bool customRetainedEnabled_ = true;
    String customTagPrefix_ = "Custom";

    static const char* levelToString_(Level level)
    {
        switch (level) {
        case Level::Fatal:
            return "FATAL";
        case Level::Error:
            return "ERROR";
        case Level::Warn:
            return "WARN";
        case Level::Info:
            return "INFO";
        case Level::Debug:
            return "DEBUG";
        case Level::Trace:
            return "TRACE";
        default:
            return "OFF";
        }
    }

    bool shouldRetainLevel_(Level level) const
    {
        switch (level) {
        case Level::Info:
            return (retainedMask_ & RetainInfo) != 0;
        case Level::Warn:
            return (retainedMask_ & RetainWarn) != 0;
        case Level::Error:
        case Level::Fatal:
            return (retainedMask_ & RetainError) != 0;
        default:
            return false;
        }
    }

    bool isCustomTag_(const char* tag) const
    {
        if (!tag || customTagPrefix_.isEmpty()) {
            return false;
        }
        const String prefix = customTagPrefix_;
        return strncmp(tag, prefix.c_str(), prefix.length()) == 0;
    }

    String buildPayload_(Level level, const char* tag, const char* message, unsigned long timestampMs) const
    {
        String out;
        out.reserve(96);

        switch (getTimestampMode()) {
        case Output::TimestampMode::Millis:
            out += '[';
            out += String(timestampMs);
            out += "] ";
            break;
        case Output::TimestampMode::DateTime: {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 0)) {
                char buf[32];
                const String& fmt = getTimestampFormat();
                const char* formatStr = fmt.length() ? fmt.c_str() : "%Y-%m-%d %H:%M:%S";
                strftime(buf, sizeof(buf), formatStr, &timeinfo);
                out += '[';
                out += buf;
                out += "] ";
            } else {
                out += '[';
                out += String(timestampMs);
                out += "] ";
            }
            break;
        }
        default:
            break;
        }

        out += '[';
        out += levelToString_(level);
        out += "] ";

        if (tag && tag[0]) {
            out += '[';
            out += tag;
            out += "] ";
        }

        const String& prefix = getPrefix();
        if (prefix.length()) {
            out += prefix;
        }

        out += (message ? message : "");
        return out;
    }
};

} // namespace cm
