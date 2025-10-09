#pragma once

// Feature toggle defaults for ConfigManager. Override by defining macros before including ConfigManager.h.
// Example:
//   #define CM_ENABLE_RUNTIME_CONTROLS 0
//   #include "ConfigManager.h"


#ifndef CM_EMBED_WEBUI // Enable embedded web UI (HTML/CSS/JS) in binary
#define CM_EMBED_WEBUI 1 //default: enabled
#endif
#if defined(CM_ALARM_GREEN_ON_FALSE)
#undef CM_ALARM_GREEN_ON_FALSE
#endif
#if defined(CM_ENABLE_RUNTIME_CONTROLS)
#undef CM_ENABLE_RUNTIME_CONTROLS
#endif
#ifndef CM_ENABLE_RUNTIME_BUTTONS // Enable runtime buttons
#define CM_ENABLE_RUNTIME_BUTTONS 1 //default: disabled
#endif
#ifndef CM_ENABLE_RUNTIME_CHECKBOXES // Enable runtime checkboxes (two-state toggles styled as android switches)
#define CM_ENABLE_RUNTIME_CHECKBOXES 1 //default: disabled
#endif
#ifndef CM_ENABLE_RUNTIME_STATE_BUTTONS // Enable runtime state buttons (ON/OFF with 2 states)
#define CM_ENABLE_RUNTIME_STATE_BUTTONS 0 //default: disabled
#endif
#ifndef CM_ENABLE_RUNTIME_INT_SLIDERS // Enable runtime integer sliders
#define CM_ENABLE_RUNTIME_INT_SLIDERS 0 //default: disabled
#endif
#ifndef CM_ENABLE_RUNTIME_FLOAT_SLIDERS // Enable runtime float sliders
#define CM_ENABLE_RUNTIME_FLOAT_SLIDERS 0 //default: disabled
#endif
// New combined flag: analog (numeric) sliders (int or float). Prefer this one in builds.
#ifndef CM_ENABLE_RUNTIME_ANALOG_SLIDERS
#define CM_ENABLE_RUNTIME_ANALOG_SLIDERS (CM_ENABLE_RUNTIME_INT_SLIDERS || CM_ENABLE_RUNTIME_FLOAT_SLIDERS)
#endif
#ifndef CM_ENABLE_RUNTIME_ALARMS // Enable runtime alarms (thresholds, color coding)
#define CM_ENABLE_RUNTIME_ALARMS 1  //default: disabled
#endif
#ifndef CM_ENABLE_RUNTIME_META // Enable generation of runtime metadata consumed by the web UI (/runtime_meta.json)
#define CM_ENABLE_RUNTIME_META (CM_ENABLE_RUNTIME_ALARMS || CM_ENABLE_SYSTEM_PROVIDER)
#endif
#ifndef CM_ENABLE_SYSTEM_PROVIDER // Enable system info runtime provider card (heap, uptime, etc.)
#define CM_ENABLE_SYSTEM_PROVIDER 1 //default: enabled
#endif
#ifndef CM_ENABLE_WS_PUSH // Enable WebSocket push of runtime JSON updates (if runtime controls or alarms are enabled, this is auto-enabled)
#define CM_ENABLE_WS_PUSH 1 //default: disabled
#endif
#ifndef CM_ENABLE_STYLE_RULES //only style rules over .set("background", "#000") etc.
#define CM_ENABLE_STYLE_RULES 0
#endif
#ifndef CM_ENABLE_USER_CSS // Enable user CSS support (you can overload all CSS served by the frontend via cfg.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);)
#define CM_ENABLE_USER_CSS 0
#endif
#ifndef CM_ENABLE_THEMING //disable both theming and user CSS for simplicity
#define CM_ENABLE_THEMING (CM_ENABLE_STYLE_RULES || CM_ENABLE_USER_CSS)
#endif
// Dynamic visibility is always enabled now
#undef CM_ENABLE_DYNAMIC_VISIBILITY
#define CM_ENABLE_DYNAMIC_VISIBILITY 1
#ifndef CM_ENABLE_OTA // Enable OTA update functionality
#define CM_ENABLE_OTA 1 //default: enabled
#endif

#ifndef CM_ENABLE_LOGGING // Enable logging via callback function (setLogger)
#define CM_ENABLE_LOGGING 0 //default: disabled
#endif

#ifndef CM_ENABLE_VERBOSE_LOGGING // Enable verbose logging (more detailed messages)
#define CM_ENABLE_VERBOSE_LOGGING 0 //default: disabled
#endif
// ---------------------------------------------------------------------------------------------------------------------
