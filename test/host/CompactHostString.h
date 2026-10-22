// [MOCKED!] Host Arduino String adapter; production storage is included unchanged.
#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <utility>
class String {
protected:
  bool isSSO() const {
    const auto p = reinterpret_cast<uintptr_t>(value_.c_str());
    const auto start = reinterpret_cast<uintptr_t>(&value_);
    return p >= start && p < start + sizeof(value_);
  }
  size_t capacity() const {
    return value_.capacity();
  }

private:
  std::string value_;

public:
  inline static int failAfter = -1;
  inline static bool failOnce = false;
  String() = default;
  // Preserve implicit Arduino String conversion in the production-facing host adapter.
  // cppcheck-suppress noExplicitConstructor
  String(const char* s)
      : value_(s ? s : "") {}
  String(const char* s, unsigned n)
      : value_(s, n) {}
  String(const String&) = default;
  String(String&&) noexcept = default;
  String& operator=(const String&) = default;
  String& operator=(String&& other) noexcept {
    if (this == &other)
      return *this;
    // Pinned Arduino String reuses destination capacity on move assignment.
    if (value_.capacity() >= other.value_.length()) {
      value_.assign(other.value_);
      other.value_.clear();
    } else
      value_ = std::move(other.value_);
    return *this;
  }
  bool reserve(unsigned n) {
    if (failAfter == 0) {
      if (failOnce)
        failAfter = -1;
      return false;
    }
    if (failAfter > 0)
      --failAfter;
    value_.reserve(n);
    return true;
  }
  bool concat(const char* s, unsigned n) {
    value_.append(s, n);
    return true;
  }
  bool concat(char c) {
    value_ += c;
    return true;
  }
  size_t allocatedBytes() const {
    return isSSO() ? 0 : capacity() + 1;
  }
  unsigned length() const {
    return static_cast<unsigned>(value_.length());
  }
  const char* c_str() const {
    return value_.c_str();
  }
  bool isEmpty() const {
    return value_.empty();
  }
  void remove(unsigned index = 0) {
    if (index < value_.size())
      value_.erase(index);
  }
  int indexOf(const String& s) const {
    auto n = value_.find(s.value_);
    return n == std::string::npos ? -1 : static_cast<int>(n);
  }
  String& operator+=(const String& s) {
    value_ += s.value_;
    return *this;
  }
  bool operator<(const String& s) const {
    return value_ < s.value_;
  }
  bool operator!=(const String& s) const {
    return !(*this == s);
  }
  bool operator!=(const char* s) const {
    return !(*this == s);
  }
  String& operator+=(const char* s) {
    value_ += s;
    return *this;
  }
  bool operator==(const String& s) const {
    return value_.length() == s.value_.length() && !std::strcmp(c_str(), s.c_str());
  }
  bool operator==(const char* s) const {
    return !std::strcmp(c_str(), s);
  }
};
