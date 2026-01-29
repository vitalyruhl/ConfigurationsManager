# Logging

This library provides two layers of logging:

- Core macros (`CM_LOG`, `CM_LOG_VERBOSE`) that are always available in modules.
- Optional advanced logging via `cm::LoggingManager` (multiple outputs, GUI log view, rate limiting).

## Core logging macros

Use the standard macros in your code and in modules:

```cpp
CM_LOG("[WiFi] Station Mode: http://%s", ip.c_str());
CM_LOG_VERBOSE("[MQTT] RX: %s", topic);
```

You can also define module-level shortcuts to keep tags consistent:

```cpp
#define WIFI_LOG(...) CM_LOG("[WiFi] " __VA_ARGS__)
#define WIFI_LOG_VERBOSE(...) CM_LOG_VERBOSE("[WiFi] " __VA_ARGS__)
```

## Build flags

Logging is controlled via build flags:

- `CM_ENABLE_LOGGING` (default: `1`)
- `CM_ENABLE_VERBOSE_LOGGING` (default: `0`)
- `CM_DISABLE_GUI_LOGGING` (default: `0`, set to `1` to remove `GuiOutput`)
- `CM_LOGGING_LEVEL` (default: `CM_LOG_LEVEL_INFO` in release, `CM_LOG_LEVEL_TRACE` in debug)

Available levels:

- `CM_LOG_LEVEL_OFF`
- `CM_LOG_LEVEL_FATAL`
- `CM_LOG_LEVEL_ERROR`
- `CM_LOG_LEVEL_WARN`
- `CM_LOG_LEVEL_INFO`
- `CM_LOG_LEVEL_DEBUG`
- `CM_LOG_LEVEL_TRACE`

Example (PlatformIO):

```ini
build_flags =
  -DCM_ENABLE_LOGGING=1
  -DCM_ENABLE_VERBOSE_LOGGING=0
  -DCM_LOGGING_LEVEL=CM_LOG_LEVEL_INFO
```

## Advanced logging (LoggingManager)

`LoggingManager` can route logs to multiple outputs with independent levels and formatting.

Minimal setup (Serial + ConfigManager hook):

```cpp
#include <logging/LoggingManager.h>

using LL = cm::LoggingManager::Level;

void setup() {
  
  Serial.begin(115200);

  auto& logManager = cm::LoggingManager::instance();

  auto serialOut = std::make_unique<cm::LoggingManager::SerialOutput>(Serial);
  serialOut->setLevel(LL::Trace);
  serialOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::Millis); //delete this line to disable timestamp completely
  logManager.addOutput(std::move(serialOut));

  logManager.attachToConfigManager(LL::Info, LL::Trace, "CM");
}

void loop() {
  logManager.loop();
}
```

### Outputs

Available outputs:

- `SerialOutput` (Print-based, e.g. Serial)
- `GuiOutput` (WebUI live log tab via WebSocket)

Each output supports:

- `setLevel(Level level)`
- `setFormat(Format format)` (`Full` or `Compact`)
- `setPrefix(const char* prefix)`
- `setMinIntervalMs(uint32_t ms)` (rate limit)
- `setFilter(std::function<bool(Level,const char*,const char*)>)`
- `addTimestamp(TimestampMode mode)` (`None`, `Millis`, `DateTime`)
- `setTimestampFormat("%Y-%m-%d %H:%M:%S")` (optional, DateTime only)

### GUI logging

`GuiOutput` sends log entries over WebSocket. The WebUI log tab is visible when log data is received.

```cpp
auto guiOut = std::make_unique<cm::LoggingManager::GuiOutput>(ConfigManager, 30);
// guiOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
logManager.addOutput(std::move(guiOut));
```

- `GuiOutput` keeps a small startup buffer (default: 30 messages) and flushes it when the first WebSocket client connects.
- Pending queue is capped and flushed incrementally to avoid blocking.
- Call `logManager.loop()` in your main loop to flush the pending queue.

### Tags

You can tag logs in several ways:

- Explicit tag:
  ```cpp
  logManager.logTag(LL::Info, "WiFi", "Connected");
  ```
- Scoped tag (auto-pop on scope exit):
  ```cpp
  auto scoped = logManager.scopedTag("SETUP");
  logManager.log(LL::Info, "Starting...");
  ```
- Message prefix in brackets (auto-extracted if no tag is provided):
  ```cpp
  CM_LOG("[MQTT] Connected");
  ```

Tags are combined with `/` if a base tag and scoped tags are active. Base tag must be changed/reset manually.

### DateTime timestamps

`TimestampMode::DateTime` uses `getLocalTime()`. If NTP is not synced yet, timestamps fall back to milliseconds so logs still contain a time value.

## Examples

- `examples/Full-Logging-Demo`

## Public API overview

### LoggingManager

- `static LoggingManager& instance()` (1x) - Singleton instance.
- `void addOutput(std::unique_ptr<Output> output)` (1x) - Add output target.
- `void clearOutputs()` (1x) - Remove all outputs.
- `void setGlobalLevel(Level level)` (1x) - Set global threshold.
- `Level getGlobalLevel() const` (1x) - Read global threshold.
- `void setTag(const char* tag)` (1x) - Set base tag.
- `void clearTag()` (1x) - Clear base tag.
- `void pushTag(const char* tag)` / `void popTag()` (1x each) - Push/pop scoped tags.
- `void addTag(const char* tag)` / `void removeTag()` (1x each) - Aliases for push/pop.
- `ScopedTag scopedTag(const char* tag)` (1x) - RAII tag scope.
- `void logTag(Level level, const char* tag, const char* format, ...)` (1x) - Log with explicit tag.
- `void log(Level level, const char* format, ...)` (1x) - Log with explicit level.
- `void log(const char* format, ...)` (1x) - Log with global level.
- `void logV(Level level, const char* tag, const char* format, va_list args)` (1x) - va_list variant.
- `void loop()` (1x) - Flush/tick outputs.
- `void attachToConfigManager(Level level, Level verboseLevel, const char* tag)` (1x) - Hook CM_LOG + verbose.
- `void attachToConfigManager(Level level, const char* tag)` (1x) - Shortcut (verbose = Trace).

### Output (base class)

- `void log(Level level, const char* tag, const char* message, unsigned long timestampMs)` (1x, pure) - Output handler.
- `void tick(unsigned long nowMs)` (1x) - Optional periodic work.
- `void setLevel(Level level)` / `Level getLevel() const` (1x each) - Output threshold.
- `void setFormat(Format format)` / `Format getFormat() const` (1x each) - Full/Compact output.
- `void setPrefix(const char* prefix)` / `const String& getPrefix() const` (1x each) - Prefix text.
- `void setTimestampMode(TimestampMode mode)` / `TimestampMode getTimestampMode() const` (1x each) - Timestamp policy.
- `void setTimestampFormat(const char* fmt)` / `const String& getTimestampFormat() const` (1x each) - DateTime format.
- `void setMinIntervalMs(uint32_t ms)` / `uint32_t getMinIntervalMs() const` (1x each) - Rate limit.
- `void setFilter(std::function<bool(Level,const char*,const char*)> fn)` (1x) - Custom filter.
- `bool shouldLog(Level level, const char* tag, const char* message) const` (1x) - Filter check.

### SerialOutput

- `SerialOutput(Print& serial = Serial)` (1x) - Serial target.
- `void addTimestamp(TimestampMode mode)` (1x) - Shortcut for timestamp mode.
- `void setRateLimitMs(uint32_t ms)` (1x) - Shortcut for output rate limit.
- `void log(Level level, const char* tag, const char* message, unsigned long timestampMs)` (1x) - Serial log handler.

### GuiOutput

- `GuiOutput(ConfigManagerClass& configManager, size_t startupBufferSize = 30)` (1x) - WebSocket output.
- `void addTimestamp(TimestampMode mode)` (1x) - Shortcut for timestamp mode.
- `void log(Level level, const char* tag, const char* message, unsigned long timestampMs)` (1x) - Queue WS log entry.
- `void tick(unsigned long nowMs)` (1x) - Flush pending WS entries.
- `void setMaxQueue(size_t limit)` (1x) - Pending queue cap.
- `void setMaxPerTick(size_t count)` (1x) - Throttle per tick.
