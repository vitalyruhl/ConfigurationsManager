#pragma once

// v3.0.0+: Most feature build flags were removed to simplify integration.
// The library provides a single default feature set (WebUI, OTA, runtime controls, styling).
// Supported build flags (defaults shown):
// - CM_EMBED_WEBUI (1)
// - CM_ENABLE_LOGGING (0)
// - CM_ENABLE_VERBOSE_LOGGING (0)
// - CM_ENABLE_OTA (1)
// - CM_ENABLE_SYSTEM_PROVIDER (1)
// - CM_ENABLE_SYSTEM_TIME (1)
// - CM_ENABLE_STYLE_RULES (1)
// - CM_ENABLE_USER_CSS (1)
// - CM_ENABLE_THEMING (1)

// --- Removed flags (error if a project still defines them) ---
#ifdef CM_ENABLE_WS_PUSH
#error "CM_ENABLE_WS_PUSH was removed in v3.0.0. WebSocket push is always enabled."
#endif

// --- Defaults (configurable via build flags) ---
#ifndef CM_EMBED_WEBUI
#define CM_EMBED_WEBUI 1
#endif
#define CM_ENABLE_WS_PUSH 1

#ifndef CM_ENABLE_SYSTEM_PROVIDER
#define CM_ENABLE_SYSTEM_PROVIDER 1
#endif

#ifndef CM_ENABLE_SYSTEM_TIME
#define CM_ENABLE_SYSTEM_TIME 1
#endif

#ifndef CM_ENABLE_STYLE_RULES
#define CM_ENABLE_STYLE_RULES 1
#endif

#ifndef CM_ENABLE_USER_CSS
#define CM_ENABLE_USER_CSS 1
#endif

#ifndef CM_ENABLE_THEMING
#define CM_ENABLE_THEMING 1
#endif

#ifndef CM_ENABLE_OTA
#define CM_ENABLE_OTA 1
#endif

// --- Logging flags (still configurable) ---
#ifndef CM_ENABLE_LOGGING
#define CM_ENABLE_LOGGING 0
#endif

#ifndef CM_ENABLE_VERBOSE_LOGGING
#define CM_ENABLE_VERBOSE_LOGGING 0
#endif

// --- Logging level defaults (override with -DCM_LOGGING_LEVEL=CM_LOG_LEVEL_*) ---
#define CM_LOG_LEVEL_OFF   0
#define CM_LOG_LEVEL_FATAL 1
#define CM_LOG_LEVEL_ERROR 2
#define CM_LOG_LEVEL_WARN  3
#define CM_LOG_LEVEL_INFO  4
#define CM_LOG_LEVEL_DEBUG 5
#define CM_LOG_LEVEL_TRACE 6

#ifndef CM_LOGGING_LEVEL
#if CM_ENABLE_VERBOSE_LOGGING
#define CM_LOGGING_LEVEL CM_LOG_LEVEL_TRACE
#else
#define CM_LOGGING_LEVEL CM_LOG_LEVEL_INFO
#endif
#endif
// ---------------------------------------------------------------------------------------------------------------------
// Dynamic visibility is always enabled - legacy code removal completed
// All related dynamic visibility flags are now obsolete and have been removed
