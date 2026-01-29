#pragma once

#include <Arduino.h>
#include <cstdarg>
#include <memory>
#include <functional>
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
        enum class TimestampMode : uint8_t {
            None = 0,
            Millis = 1,
            DateTime = 2,
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

        void setTimestampMode(TimestampMode mode) { timestampMode_ = mode; }
        TimestampMode getTimestampMode() const { return timestampMode_; }
        void setTimestampFormat(const char* fmt) { timestampFormat_ = fmt ? String(fmt) : String(); }
        const String& getTimestampFormat() const { return timestampFormat_; }

        void setMinIntervalMs(uint32_t ms) { minIntervalMs_ = ms; }
        uint32_t getMinIntervalMs() const { return minIntervalMs_; }

        void setFilter(std::function<bool(Level level, const char* tag, const char* message)> fn) { filter_ = std::move(fn); }
        bool shouldLog(Level level, const char* tag, const char* message) const
        {
            if (filter_) {
                return filter_(level, tag, message);
            }
            return true;
        }

    protected:
        bool allowRate(unsigned long timestampMs)
        {
            if (minIntervalMs_ == 0) {
                return true;
            }
            if (timestampMs - lastLogMs_ < minIntervalMs_) {
                return false;
            }
            lastLogMs_ = timestampMs;
            return true;
        }

    private:
        Level level_ = Level::Info;
        Format format_ = Format::Full;
        TimestampMode timestampMode_ = TimestampMode::None;
        String timestampFormat_;
        uint32_t minIntervalMs_ = 0;
        unsigned long lastLogMs_ = 0;
        String prefix_;
        std::function<bool(Level level, const char* tag, const char* message)> filter_;
    };

    class SerialOutput : public Output {
    public:
        explicit SerialOutput(Print& serial = Serial)
            : serial_(serial)
        {
        }

        void addTimestamp(TimestampMode mode) { setTimestampMode(mode); }
        void setRateLimitMs(uint32_t ms) { setMinIntervalMs(ms); }
        void log(Level level, const char* tag, const char* message, unsigned long timestampMs) override;

    private:
        Print& serial_;
    };

#if !CM_DISABLE_GUI_LOGGING
    class GuiOutput : public Output {
    public:
        GuiOutput(ConfigManagerClass& configManager, size_t startupBufferSize = 30);
        void addTimestamp(TimestampMode mode) { setTimestampMode(mode); }
        void log(Level level, const char* tag, const char* message, unsigned long timestampMs) override;
        void tick(unsigned long nowMs) override;
        void setMaxQueue(size_t limit) { pendingLimit_ = limit; }
        void setMaxPerTick(size_t count) { maxPerTick_ = count; }

    private:
        ConfigManagerClass& configManager_;
        size_t bufferLimit_ = 30;
        bool bufferEnabled_ = true;
        std::vector<String> buffer_;
        std::vector<String> pending_;
        size_t pendingLimit_ = 200;
        size_t maxPerTick_ = 8;
        String makeReadyPayload_() const;
        void enqueue_(const String& payload);
        void enqueuePending_(const String& payload);
        void flushToClient_(AsyncWebSocketClient* client);
        static String escapeJson_(const char* value);
        String makePayload_(Level level, const char* tag, const char* message, unsigned long timestampMs) const;
    };
#endif

    static LoggingManager& instance()
    {
        static LoggingManager inst;
        return inst;
    }

    void addOutput(std::unique_ptr<Output> output);
    void clearOutputs();

    void setGlobalLevel(Level level) { globalLevel_ = level; }
    Level getGlobalLevel() const { return globalLevel_; }
    void setTag(const char* tag);
    void clearTag();
    void pushTag(const char* tag);
    void popTag();
    void addTag(const char* tag) { pushTag(tag); }
    void removeTag() { popTag(); }

    class ScopedTag {
    public:
        ScopedTag(LoggingManager* mgr, const char* tag) : mgr_(mgr) { if (mgr_) mgr_->pushTag(tag); }
        ~ScopedTag() { if (mgr_) mgr_->popTag(); }
        ScopedTag(const ScopedTag&) = delete;
        ScopedTag& operator=(const ScopedTag&) = delete;
        ScopedTag(ScopedTag&& other) noexcept : mgr_(other.mgr_) { other.mgr_ = nullptr; }
        ScopedTag& operator=(ScopedTag&& other) noexcept
        {
            if (this != &other) {
                if (mgr_) {
                    mgr_->popTag();
                }
                mgr_ = other.mgr_;
                other.mgr_ = nullptr;
            }
            return *this;
        }
    private:
        LoggingManager* mgr_;
    };
    ScopedTag scopedTag(const char* tag) { return ScopedTag(this, tag); }

    void logTag(Level level, const char* tag, const char* format, ...);
    void log(Level level, const char* format, ...);
    void log(const char* format, ...);
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
    String baseTag_;
    std::vector<String> tagStack_;
    String buildTag_(const char* tag) const;
};

} // namespace cm
