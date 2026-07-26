// Generated WebUI content wrapper
#include <Arduino.h>
#include "ConfigManagerConfig.h"
#if CM_EMBED_WEBUI
#include "html_content.h"
// Provide implementations for gzipped accessors
// Cppcheck rationale: Preserve the existing instance API for source compatibility.
// cppcheck-suppress functionStatic
const uint8_t* WebHTML::getWebHTMLGz() {
  return WEB_HTML_GZ;
}
// Cppcheck rationale: Preserve the existing instance API for source compatibility.
// cppcheck-suppress functionStatic
size_t WebHTML::getWebHTMLGzLen() {
  return WEB_HTML_GZ_LEN;
}
#endif
