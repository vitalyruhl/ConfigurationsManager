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
        virtual ~Output() = default;
        virtual void log(Level level, const char* tag, const char* message, unsigned long timestampMs) = 0;
        virtual void tick(unsigned long nowMs) { (void)nowMs; }

        void setLevel(Level level) { level_ = level; }
        Level getLevel() const { return level_; }

    private:
        Level level_ = Level::Info;
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

    void attachToConfigManager(Level level = Level::Info, const char* tag = "ConfigManager");

private:
    LoggingManager() = default;

    bool shouldLog_(Level level) const;

    std::vector<std::unique_ptr<Output>> outputs_;
    Level globalLevel_ = Level::Info;
    Level defaultLevel_ = Level::Info;
    const char* defaultTag_ = "ConfigManager";
};

} // namespace cm
