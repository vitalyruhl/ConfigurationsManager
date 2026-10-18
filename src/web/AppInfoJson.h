#pragma once

#include "JsonOutput.h"

namespace cm {
// Callers keep these borrowed values stable until both count/write passes end.
// Only the output string is allocated; no temporary JSON document is retained.
template<class StringLike, class Output>
bool buildAppInfoJson(const StringLike& appName, const StringLike& appTitle,
                      const StringLike& version, bool guiLogging, Output& output) {
  return buildJsonOutput([&](auto& sink) {
    bool first = true;
    return sink.raw("{") && sink.member(first, "appName", appName) &&
        sink.member(first, "appTitle", appTitle) && sink.member(first, "version", version) &&
        sink.member(first, "guiLogging", guiLogging) && sink.raw("}");
  }, output);
}
} // namespace cm
