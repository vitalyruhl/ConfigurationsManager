#pragma once

#include <ArduinoJson.h>
#include <ArduinoJson/Json/TextFormatter.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace cm {
// Bounded JSON scalar output. raw() is only for caller-controlled punctuation;
// dynamic strings always go through string()/value(). No document tree is built.
// A null output counts bytes with the same escaping and scalar formatting.
template<class Output>
class JsonOutput {
public:
  explicit JsonOutput(Output* output = nullptr,
                      size_t limit = std::numeric_limits<size_t>::max())
      : output_(output), limit_(limit) {}

  size_t size() const { return size_; }
  bool good() const { return good_; }

  bool raw(const char* text) {
    if (!good_ || !text) return fail();
    return append(text, std::strlen(text));
  }

  bool string(const char* text, size_t length) {
    if (!good_ || !text) return fail();
    // Even without escaping, the input plus quotes must fit. Check before
    // reading the input so a length overflow cannot wrap the size accounting.
    const size_t remaining = limit_ - size_;
    if (remaining < 2 || length > remaining - 2) return fail();
    if (!append("\"", 1)) return false;
    static constexpr char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < length; ++i) {
      const unsigned char byte = static_cast<unsigned char>(text[i]);
      const char* escape = nullptr;
      switch (byte) {
        case '"': escape = "\\\""; break;
        case '\\': escape = "\\\\"; break;
        case '\b': escape = "\\b"; break;
        case '\f': escape = "\\f"; break;
        case '\n': escape = "\\n"; break;
        case '\r': escape = "\\r"; break;
        case '\t': escape = "\\t"; break;
      }
      if (escape) {
        if (!append(escape, 2)) return false;
      } else if (byte < 0x20) {
        const char escaped[] = {'\\', 'u', '0', '0', hex[byte >> 4], hex[byte & 15]};
        if (!append(escaped, sizeof(escaped))) return false;
      } else if (!append(text + i, 1)) {
        return false;
      }
    }
    return append("\"", 1);
  }

  bool string(const char* text) {
    return text ? string(text, std::strlen(text)) : fail();
  }

  template<class StringLike>
  auto string(const StringLike& text) -> decltype(text.c_str(), text.length(), bool()) {
    return string(text.c_str(), text.length());
  }

  bool value(std::nullptr_t) { return raw("null"); }
  bool value(bool input) { return raw(input ? "true" : "false"); }
  bool value(const char* text) { return text ? string(text) : raw("null"); }

  template<class StringLike>
  auto value(const StringLike& text) -> decltype(text.c_str(), text.length(), bool()) {
    return string(text);
  }

  template<class Integer,
           typename std::enable_if<std::is_integral<Integer>::value &&
                                   !std::is_same<Integer, bool>::value, int>::type = 0>
  bool value(Integer input) {
    using Unsigned = typename std::make_unsigned<Integer>::type;
    Unsigned magnitude = static_cast<Unsigned>(input);
    const bool negative = std::is_signed<Integer>::value && input < 0;
    if (negative) magnitude = Unsigned(0) - magnitude;
    // Decimal digits plus a possible sign; also safe for signed minimum.
    char buffer[std::numeric_limits<Unsigned>::digits10 + 3];
    char* end = buffer + sizeof(buffer);
    char* begin = end;
    do {
      *--begin = static_cast<char>('0' + magnitude % 10);
      magnitude /= 10;
    } while (magnitude);
    if (negative) *--begin = '-';
    return append(begin, static_cast<size_t>(end - begin));
  }

  bool value(float input) { return floating(input); }
  bool value(double input) {
    // ArduinoJson 7 stores exactly representable doubles as floats. Match the
    // resulting 6-place vs 9-place formatting without allocating a variant.
    const float compact = static_cast<float>(input);
#if ARDUINOJSON_USE_DOUBLE
    return input == compact ? floating(compact) : floating(input);
#else
    return floating(compact);
#endif
  }

  template<class Key, class Value>
  bool member(bool& first, const Key& key, const Value& input) {
    if ((!first && !raw(",")) || !string(key) || !raw(":") || !value(input)) return false;
    first = false;
    return true;
  }

private:
  bool fail() { good_ = false; return false; }

  bool append(const char* bytes, size_t length) {
    if (!good_ || length > limit_ - size_) return fail();
    if (output_) {
      // The pinned Arduino String::concat(ptr,n) can read n+1 bytes. Numeric
      // and escape slices are not terminated; the character overload is safe.
      for (size_t i = 0; i < length; ++i) {
        if (!output_->concat(bytes[i])) return fail();
        ++size_;
      }
    } else {
      size_ += length;
    }
    return true;
  }

  struct NumberWriter {
    JsonOutput* sink;
    size_t write(uint8_t byte) {
      const char c = static_cast<char>(byte);
      return sink->append(&c, 1) ? 1 : 0;
    }
    size_t write(const uint8_t* bytes, size_t length) {
      const size_t before = sink->size();
      sink->append(reinterpret_cast<const char*>(bytes), length);
      return sink->size() - before;
    }
  };

  template<class Float>
  bool floating(Float input) {
    if (!good_) return false;
    // Internal dependency boundary: ArduinoJson 7.4.3 TextFormatter preserves
    // its float rounding, exponent thresholds and nonfinite-value settings.
    // It writes to this bounded adapter and never creates a JsonDocument.
    // Host scalar goldens must pass when changing the ArduinoJson version.
    ArduinoJson::detail::TextFormatter<NumberWriter> formatter(NumberWriter{this});
    formatter.writeFloat(input);
    return good_;
  }

  Output* output_;
  size_t limit_;
  size_t size_ = 0;
  bool good_ = true;
};

// Inputs must remain stable across both passes. Reserve once; any failure or
// changed length rejects the whole output rather than exposing a JSON prefix.
template<class Builder, class Output>
bool buildJsonOutput(Builder&& builder, Output& output) {
  output.remove(0);
  // Leave room for Arduino String's terminating byte in unsigned size math.
  const size_t limit = std::numeric_limits<unsigned int>::max() - size_t(1);
  JsonOutput<Output> count(nullptr, limit);
  if (!builder(count) || !count.good() ||
      !output.reserve(static_cast<unsigned int>(count.size()))) return false;
  JsonOutput<Output> writer(&output, count.size());
  if (!builder(writer) || !writer.good() || writer.size() != count.size() ||
      output.length() != count.size()) {
    output.remove(0);
    return false;
  }
  return true;
}
} // namespace cm
