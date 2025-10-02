#pragma once

// Feature toggle defaults for ConfigManager. Override by defining macros before including ConfigManager.h.
// Example:
//   #define CM_ENABLE_RUNTIME_CONTROLS 0
//   #include "ConfigManager.h"

#ifndef CM_ALARM_GREEN_ON_FALSE // Enable showing alarm state green on false for boolean runtime fields
#define CM_ALARM_GREEN_ON_FALSE 1
#endif

#ifndef CM_EMBED_WEBUI
#define CM_EMBED_WEBUI 1
#endif

#ifndef CM_ENABLE_THEMING
#define CM_ENABLE_THEMING 1
#endif

#ifndef CM_ENABLE_USER_CSS
#define CM_ENABLE_USER_CSS 0
#endif

#ifndef CM_ENABLE_STYLE_RULES
#define CM_ENABLE_STYLE_RULES 0
#endif

#ifndef CM_ENABLE_RUNTIME_META
#define CM_ENABLE_RUNTIME_META 1
#endif

#ifndef CM_ENABLE_RUNTIME_CONTROLS
#define CM_ENABLE_RUNTIME_CONTROLS 1
#endif

#ifndef CM_ENABLE_RUNTIME_BUTTONS
#define CM_ENABLE_RUNTIME_BUTTONS 0
#endif

#ifndef CM_ENABLE_RUNTIME_CHECKBOXES
#define CM_ENABLE_RUNTIME_CHECKBOXES 0
#endif

#ifndef CM_ENABLE_RUNTIME_STATE_BUTTONS
#define CM_ENABLE_RUNTIME_STATE_BUTTONS 0
#endif

#ifndef CM_ENABLE_RUNTIME_INT_SLIDERS
#define CM_ENABLE_RUNTIME_INT_SLIDERS 0
#endif

#ifndef CM_ENABLE_RUNTIME_FLOAT_SLIDERS
#define CM_ENABLE_RUNTIME_FLOAT_SLIDERS 0
#endif

#ifndef CM_ENABLE_ADV_CONTROLS
#define CM_ENABLE_ADV_CONTROLS 0
#endif

#ifndef CM_ENABLE_RUNTIME_ALARMS
#define CM_ENABLE_RUNTIME_ALARMS 0
#endif

#ifndef CM_ENABLE_WS_PUSH
#define CM_ENABLE_WS_PUSH 0
#endif

#ifndef CM_ENABLE_SYSTEM_PROVIDER
#define CM_ENABLE_SYSTEM_PROVIDER 0
#endif

#ifndef CM_ENABLE_DYNAMIC_VISIBILITY
#define CM_ENABLE_DYNAMIC_VISIBILITY 1
#endif

#ifndef CM_ENABLE_OTA
#define CM_ENABLE_OTA 0
#endif

#ifndef CM_ENABLE_LOGGING
#define CM_ENABLE_LOGGING 0
#endif

#ifndef CM_ENABLE_VERBOSE_LOGGING
#define CM_ENABLE_VERBOSE_LOGGING 0
#endif
