#pragma once

#include <cstddef>
#include <limits>

namespace cm {
// Custom CSS is app-owned and may change after this request. Capture exact bytes
// before handing the response to the asynchronous transport; reject partial copies.
template <class Output>
bool snapshotThemeCss(const char* css, size_t length, Output& output) {
  output.remove(0);
  if (!length)
    return true;
  if (!css || length > std::numeric_limits<unsigned int>::max() - size_t(1) ||
      !output.reserve(static_cast<unsigned int>(length)))
    return false;
  // The pinned Arduino String concat(ptr,n) may read n+1 bytes. Append characters
  // into the reserved buffer so an exact-length CSS buffer needs no terminator.
  for (size_t i = 0; i < length; ++i) {
    if (!output.concat(css[i])) {
      output.remove(0);
      return false;
    }
  }
  if (output.length() != length) {
    output.remove(0);
    return false;
  }
  return true;
}
} // namespace cm
