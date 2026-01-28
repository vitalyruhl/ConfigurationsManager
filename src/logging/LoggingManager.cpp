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
    serial_.print('[');
    serial_.print(timestampMs);
    serial_.print("] ");
    serial_.print('[');
    serial_.print(levelToString(level));
    serial_.print("] ");
    if (tag && tag[0]) {
        serial_.print('[');
        serial_.print(tag);
        serial_.print("] ");
    }
    serial_.println(message ? message : "");
}

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

void LoggingManager::log(Level level, const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    logV(level, tag, format, args);
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
    for (auto& output : outputs_) {
        if (output) {
            output->log(level, tag, buffer, ts);
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

void LoggingManager::attachToConfigManager(Level level, const char* tag)
{
    defaultLevel_ = level;
    defaultTag_ = (tag && tag[0]) ? tag : "ConfigManager";

    ConfigManagerClass::setLogger([this](const char* msg) {
        this->log(defaultLevel_, defaultTag_, "%s", msg ? msg : "");
    });
}

bool LoggingManager::shouldLog_(Level level) const
{
    if (level == Level::Off || globalLevel_ == Level::Off) {
        return false;
    }
    return level <= globalLevel_;
}

} // namespace cm
