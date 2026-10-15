#pragma once
#include "BoundedResponse.h"

namespace cm {
// ESP32 PROGMEM is memory mapped. The immutable asset outlives the request.
class StaticGzipResponse final : public BoundedResponse {
public:
  StaticGzipResponse(const uint8_t* content, size_t length)
      : BoundedResponse(content, length, "text/html") {
    addHeader("Content-Encoding", "gzip");
  }
  bool _sourceValid() const override { return _contentLength != 0 && BoundedResponse::_sourceValid(); }
};
} // namespace cm
