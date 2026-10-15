#pragma once
#include <ArduinoJson.h>
#include <limits>


namespace cm {
enum class CheckedJsonReason { Ok, Overflow, Size, Reserve, Write, Length };
struct CheckedJsonResult {
  CheckedJsonReason reason = CheckedJsonReason::Ok;
  size_t expected = 0;
  size_t written = 0;
  size_t stored = 0;
};

template <typename Output>
class CheckedJsonWriter {
public:
  explicit CheckedJsonWriter(Output& output) : output_(output) {}
  size_t write(uint8_t byte) {
    if (failed_ || !output_.concat(static_cast<char>(byte))) {
      failed_ = true;
      return 0;
    }
    return 1;
  }
  size_t write(const uint8_t* bytes, size_t size) {
    // Pinned Arduino String::concat(ptr, size) reads size+1 bytes. JSON number
    // slices need not be terminated, so use the safe character overload.
    size_t written = 0;
    while (written < size && write(bytes[written])) ++written;
    return written;
  }
  bool failed() const { return failed_; }
private:
  Output& output_;
  bool failed_ = false;
};

// ArduinoJson's String writer clears reserved storage and can leave a prefix on
// allocation failure. This writer preserves the reservation and checks every append.
template <typename Document, typename Output>
bool serializeJsonChecked(const Document& document, Output& output, CheckedJsonResult* result = nullptr) {
  CheckedJsonResult measured;
  const auto finish = [&](CheckedJsonReason reason) {
    measured.reason = reason;
    measured.stored = output.length();
    if (result) *result = measured;
    if (reason != CheckedJsonReason::Ok) output.remove(0);
    return reason == CheckedJsonReason::Ok;
  };
  output.remove(0);
  if (document.overflowed()) return finish(CheckedJsonReason::Overflow);
  measured.expected = measureJson(document);
  if (measured.expected > std::numeric_limits<unsigned int>::max()) return finish(CheckedJsonReason::Size);
  if (!output.reserve(static_cast<unsigned int>(measured.expected))) return finish(CheckedJsonReason::Reserve);
  CheckedJsonWriter<Output> writer(output);
  measured.written = serializeJson(document, writer);
  // ArduinoJson adapters may copy a writer; count and actual length remain
  // authoritative even if a copied writer owns its failure flag.
  if (writer.failed() || measured.written != measured.expected) return finish(CheckedJsonReason::Write);
  if (output.length() != measured.expected) return finish(CheckedJsonReason::Length);
  return finish(CheckedJsonReason::Ok);
}

} // namespace cm
