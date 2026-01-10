#pragma once

// v3.0.0: Feature build flags were removed to simplify integration.
// The library now provides a single default feature set (WebUI, OTA, runtime controls, styling).
// Only logging remains configurable via build flags.

// --- Removed flags (error if a project still defines them) ---
#ifdef CM_EMBED_WEBUI
#error "CM_EMBED_WEBUI was removed in v3.0.0. WebUI is always embedded."
#endif
#ifdef CM_ENABLE_RUNTIME_CHECKBOXES
#error "CM_ENABLE_RUNTIME_CHECKBOXES was removed in v3.0.0. Runtime checkboxes are always enabled."
#endif
#ifdef CM_ENABLE_RUNTIME_BUTTONS
#error "CM_ENABLE_RUNTIME_BUTTONS was removed in v3.0.0. Runtime buttons are always enabled."
#endif
#ifdef CM_ENABLE_RUNTIME_STATE_BUTTONS
#error "CM_ENABLE_RUNTIME_STATE_BUTTONS was removed in v3.0.0. Runtime state buttons are always enabled."
#endif
#ifdef CM_ENABLE_RUNTIME_INT_SLIDERS
#error "CM_ENABLE_RUNTIME_INT_SLIDERS was removed in v3.0.0. Use the default runtime controls."
#endif
#ifdef CM_ENABLE_RUNTIME_FLOAT_SLIDERS
#error "CM_ENABLE_RUNTIME_FLOAT_SLIDERS was removed in v3.0.0. Use the default runtime controls."
#endif
#ifdef CM_ENABLE_RUNTIME_ANALOG_SLIDERS
#error "CM_ENABLE_RUNTIME_ANALOG_SLIDERS was removed in v3.0.0. Runtime sliders are always enabled."
#endif
#ifdef CM_ENABLE_RUNTIME_NUMBER_INPUTS
#error "CM_ENABLE_RUNTIME_NUMBER_INPUTS was removed in v3.0.0. Runtime number inputs are always enabled."
#endif
#ifdef CM_ENABLE_RUNTIME_ALARMS
#error "CM_ENABLE_RUNTIME_ALARMS was removed in v3.0.0. Runtime alarms are always enabled."
#endif
#ifdef CM_ENABLE_RUNTIME_META
#error "CM_ENABLE_RUNTIME_META was removed in v3.0.0. Runtime metadata is always enabled."
#endif
#ifdef CM_ENABLE_SYSTEM_PROVIDER
#error "CM_ENABLE_SYSTEM_PROVIDER was removed in v3.0.0. System provider is always enabled."
#endif
#ifdef CM_ENABLE_SYSTEM_TIME
#error "CM_ENABLE_SYSTEM_TIME was removed in v3.0.0. System time support is always enabled."
#endif
#ifdef CM_ENABLE_WS_PUSH
#error "CM_ENABLE_WS_PUSH was removed in v3.0.0. WebSocket push is always enabled."
#endif
#ifdef CM_ENABLE_STYLE_RULES
#error "CM_ENABLE_STYLE_RULES was removed in v3.0.0. Styling is always enabled."
#endif
#ifdef CM_ENABLE_USER_CSS
#error "CM_ENABLE_USER_CSS was removed in v3.0.0. User CSS support is always enabled."
#endif
#ifdef CM_ENABLE_THEMING
#error "CM_ENABLE_THEMING was removed in v3.0.0. Theming is always enabled."
#endif
#ifdef CM_ENABLE_OTA
#error "CM_ENABLE_OTA was removed in v3.0.0. OTA is always enabled."
#endif

// --- Fixed defaults (not configurable anymore) ---
#define CM_EMBED_WEBUI 1
#define CM_ENABLE_RUNTIME_CHECKBOXES 1
#define CM_ENABLE_RUNTIME_BUTTONS 1
#define CM_ENABLE_RUNTIME_STATE_BUTTONS 1

#define CM_ENABLE_RUNTIME_ANALOG_SLIDERS 1

#define CM_ENABLE_RUNTIME_NUMBER_INPUTS 1
#define CM_ENABLE_RUNTIME_ALARMS 1
#define CM_ENABLE_SYSTEM_PROVIDER 1
#define CM_ENABLE_SYSTEM_TIME 1
#define CM_ENABLE_WS_PUSH 1
#define CM_ENABLE_STYLE_RULES 1
#define CM_ENABLE_USER_CSS 1
#define CM_ENABLE_THEMING 1
#define CM_ENABLE_RUNTIME_META 1

#define CM_ENABLE_OTA 1

// --- Logging flags (still configurable) ---
#ifndef CM_ENABLE_LOGGING
#define CM_ENABLE_LOGGING 1
#endif

#ifndef CM_ENABLE_VERBOSE_LOGGING
#define CM_ENABLE_VERBOSE_LOGGING 0
#endif
// ---------------------------------------------------------------------------------------------------------------------
// Dynamic visibility is always enabled - legacy code removal completed
// All related dynamic visibility flags are now obsolete and have been removed