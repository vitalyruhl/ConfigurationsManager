#pragma once

#include <Arduino.h>
#include <cstdarg>
#include <memory>
#include <vector>

#include "../ConfigManager.h"

namespace cm {

class LoggingManager {
public:
    enum class Level : uint8_t {
        Off = 0,
        Fatal = 1,
        Error = 2,
        Warn = 3,
        Info = 4,
        Debug = 5,
        Trace = 6,
    };

    class Output {
    public:
        enum class Format : uint8_t {
            Full = 0,
            Compact = 1,
        };

        virtual ~Output() = default;
        virtual void log(Level level, const char* tag, const char* message, unsigned long timestampMs) = 0;
        virtual void tick(unsigned long nowMs) { (void)nowMs; }

        void setLevel(Level level) { level_ = level; }
        Level getLevel() const { return level_; }

        void setFormat(Format format) { format_ = format; }
        Format getFormat() const { return format_; }

        void setPrefix(const char* prefix) { prefix_ = prefix ? String(prefix) : String(); }
        const String& getPrefix() const { return prefix_; }

        void setFilter(std::function<bool(Level level, const char* tag, const char* message)> fn) { filter_ = std::move(fn); }
        bool shouldLog(Level level, const char* tag, const char* message) const
        {
            if (filter_) {
                return filter_(level, tag, message);
            }
            return true;
        }

    private:
        Level level_ = Level::Info;
        Format format_ = Format::Full;
        String prefix_;
        std::function<bool(Level level, const char* tag, const char* message)> filter_;
    };

    class SerialOutput : public Output {
    public:
        explicit SerialOutput(Print& serial = Serial)
            : serial_(serial)
        {
        }

        void log(Level level, const char* tag, const char* message, unsigned long timestampMs) override;

    private:
        Print& serial_;
    };

    static LoggingManager& instance()
    {
        static LoggingManager inst;
        return inst;
    }

    void addOutput(std::unique_ptr<Output> output);
    void clearOutputs();

    void setGlobalLevel(Level level) { globalLevel_ = level; }
    Level getGlobalLevel() const { return globalLevel_; }

    void log(Level level, const char* tag, const char* format, ...);
    void logV(Level level, const char* tag, const char* format, va_list args);

    void loop();

    void attachToConfigManager(Level level = levelFromMacro_(CM_LOGGING_LEVEL),
                               Level verboseLevel = Level::Trace,
                               const char* tag = "ConfigManager");
    void attachToConfigManager(Level level, const char* tag)
    {
        attachToConfigManager(level, Level::Trace, tag);
    }

    static constexpr Level levelFromMacro_(int value)
    {
        switch (value) {
        case CM_LOG_LEVEL_FATAL:
            return Level::Fatal;
        case CM_LOG_LEVEL_ERROR:
            return Level::Error;
        case CM_LOG_LEVEL_WARN:
            return Level::Warn;
        case CM_LOG_LEVEL_INFO:
            return Level::Info;
        case CM_LOG_LEVEL_DEBUG:
            return Level::Debug;
        case CM_LOG_LEVEL_TRACE:
            return Level::Trace;
        default:
            return Level::Off;
        }
    }

private:
    LoggingManager() = default;

    bool shouldLog_(Level level) const;

    std::vector<std::unique_ptr<Output>> outputs_;
    Level globalLevel_ = Level::Info;
    Level defaultLevel_ = Level::Info;
    Level verboseLevel_ = Level::Trace;
    const char* defaultTag_ = "ConfigManager";
    const char* verboseTag_ = "ConfigManager";
};

} // namespace cm
