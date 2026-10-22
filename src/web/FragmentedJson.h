#pragma once
#include "CheckedJson.h"
#include <cstring>

namespace cm {
// Count or append complete JSON fragments without keeping their documents alive.
// Only fixed JSON punctuation belongs in raw(); dynamic values use json().
template <typename Output>
class JsonFragmentSink {
public:
  explicit JsonFragmentSink(Output* output = nullptr, size_t limit = std::numeric_limits<size_t>::max())
      : output_(output), limit_(limit) {}
  size_t size() const {
    return size_;
  }
  bool raw(const char* text) {
    const size_t length = std::strlen(text);
    if (length > limit_ - size_)
      return false;
    if (output_) {
      CheckedJsonWriter<Output> writer(*output_);
      if (writer.write(reinterpret_cast<const uint8_t*>(text), length) != length)
        return false;
    }
    size_ += length;
    return true;
  }
  bool json(const JsonDocument& document) {
    if (document.overflowed())
      return false;
    const size_t length = measureJson(document);
    if (length > limit_ - size_)
      return false;
    if (output_) {
      const size_t before = output_->length();
      CheckedJsonWriter<Output> writer(*output_);
      if (serializeJson(document, writer) != length || output_->length() - before != length)
        return false;
    }
    size_ += length;
    return true;
  }

private:
  Output* output_;
  size_t limit_;
  size_t size_ = 0;
};

template <typename Builder, typename Output>
bool buildFragmentedJson(Builder&& build, Output& output) {
  output.remove(0);
  JsonFragmentSink<Output> count;
  if (!build(count) || count.size() > std::numeric_limits<unsigned int>::max() ||
      !output.reserve(static_cast<unsigned int>(count.size())))
    return false;
  JsonFragmentSink<Output> append(&output, count.size());
  if (!build(append) || append.size() != count.size() || output.length() != count.size()) {
    output.remove(0);
    return false;
  }
  return true;
}
} // namespace cm
