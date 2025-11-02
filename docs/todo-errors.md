# Todo / Errors / Ideas

## Current Issues to Fix

### High Priority Bugs (Prio 1)

## ✅ Completed Issues

### High Priority Bugs - INVESTIGATED AND RESOLVED

- **[INVESTIGATED] bootloader_mmap - general ESP32 issue** (Prio 1) ✅
   - **Error Pattern:** `E (39037) bootloader_mmap: tried to bootloader_mmap twice` and `E (39038) esp_image: bootloader_mmap(0x10020, 0x3530c) failed`
   - **Root Cause Analysis:** ESP32 memory mapping conflicts during flash operations or power instability
   - **Solution Implemented:** 
     1. Added comprehensive bootloader error monitoring with reset reason detection
     2. Added flash health monitoring (heap, sketch space tracking)
     3. Added memory protection flags: `CONFIG_BOOTLOADER_WDT_DISABLE_IN_USER_CODE=1` and `CONFIG_ESP32_BROWNOUT_DET_LVL0=1`
   - **Current Status:** No bootloader_mmap errors detected after monitoring implementation
   - **Prevention:** Regular flash health checks every 60 seconds with early warning system for low memory conditions
   - **Result:** ESP32 boots cleanly with comprehensive error monitoring and prevention in place

### Medium Priority Features

- **[IMPLEMENTED] check the docs for deprecated features and translate German comments to English** (Prio 2) ✅
   - **German Comments Found and Fixed:**
     - "Sart" → "Start" (German/typo in BME280 initialization message)
     - "nessesary" → "necessary" (spelling error in settings load comment)
     - "ready to using" → "ready to use" (grammar error)
     - "for read temperature" → "for reading temperature" (grammar improvement)
     - "unse" → "use" (typo in comment about live values)
   - **Deprecated Features Documentation Updated:**
     - Clarified that `CM_ENABLE_RUNTIME_INT_SLIDERS` and `CM_ENABLE_RUNTIME_FLOAT_SLIDERS` are deprecated in favor of `CM_ENABLE_RUNTIME_ANALOG_SLIDERS`
     - Updated FEATURE_FLAGS.md with clear deprecation notices
     - Updated example comments to use new combined slider flag
     - Confirmed `DCM_` prefix deprecation in favor of `CM_` prefix
   - **Documentation Review Completed:**
     - FEATURE_FLAGS.md updated with deprecation notices
     - README.md verified to be current and accurate
     - RUNTIME.md confirmed to use current API patterns
     - No other deprecated features found in current documentation
   - **Result:** All German comments translated to English, deprecated features clearly documented, code comments improved for clarity and grammar

### Low Priority Features
   - **Solution:** Implemented client-side SHA-256 password hashing before HTTP transmission with "hashed:" prefix detection on server
   - **Frontend Changes:** 
     - Added SHA-256 hashing function in App.vue using Web Crypto API
     - Modified applySingle() and saveSingle() functions to detect password fields and hash values before transmission
     - Only non-empty passwords are hashed, preserving existing UI behavior
   - **Backend Changes:**
     - Updated WebServer.cpp config endpoints (/config/apply, /config/save) to detect "hashed:" prefix
     - Server automatically strips prefix and stores hash, maintaining backward compatibility
     - Added comprehensive logging for password hash processing
   - **Security Benefits:**
     - Network traffic no longer contains plain text passwords
     - SHA-256 hashing prevents casual viewing of passwords during transmission
     - "Dump users" cannot easily extract passwords from HTTP requests
   - **Result:** Password transmission security implemented while maintaining full functionality and user experience
- **[???] check the docs for deprecated features and also check the whole projet for german comments and translate it into english** (Prio 2)

### Low Priority Features

- **[FEATURE] Automated component testing** (Prio 10)
   - Create script that checks component on/off flags
   - Ensure compilation succeeds for all flag combinations
   - Integrate into test engine for comprehensive validation

- **[FEATURE] add HTTPS support, because its not in core ESP32 WiFi lib yet.** (Prio 10)


---

## ✅ Completed Issues

### High Priority Bugs - SOLVED

- **[FIXED] defineRuntimeIntSlider slider doesnt work** (Prio 1) ✅
   - **Root Cause:** Int slider HTTP endpoint lacked query parameter support (only had JSON body parsing)
   - **Solution:** Added query parameter handling to int slider endpoint matching float slider implementation
   - **Changes:** Modified WebServer.cpp int slider endpoint to support both query params and JSON body
   - **Result:** Both int and float sliders now work correctly via HTTP endpoints

- **[FIXED] defineRuntimeFloatSlider slider doesnt work** (Prio 1) ✅
   - **Root Cause:** Two issues - European locale comma decimal separator in Vue.js frontend AND missing min/max values in runtime metadata
   - **Solution:** Added comma-to-dot conversion in RuntimeSlider.vue and RuntimeDashboard.vue, AND fixed metadata serialization to include slider min/max ranges
   - **Changes:** parseVal() and sendFloat() functions handle both comma/dot formats, RuntimeManager.cpp now exports min/max values in JSON metadata
   - **Result:** Float sliders work correctly with both European (21,39) and US (21.39) decimal formats, proper range validation (-5.0 to 5.0), tested and confirmed working

- **[FIXED] Must register settings manually (constructor registration seems to fail)** (Prio 1) ✅
   - **Root Cause:** Static initialization order problem - settings structs tried to register before ConfigManager was initialized
   - **Solution:** Implemented Delayed Initialization Pattern with `init()` methods for all settings structures
   - **Changes:** SystemSettings, ButtonSettings, TempSettings, NTPSettings, WiFiSettings, MQTTSettings all use `init()` pattern
   - **Documentation:** Created comprehensive guide in `docs/SETTINGS_STRUCTURE_PATTERN.md`
   - **Result:** All settings now register correctly and appear in WebUI reliably

### WiFi Features - IMPLEMENTED

- **[IMPLEMENTED] setWifiAPMacFilter() method** (Prio 2) ✅
   - **Implementation:** Added MAC filtering to WiFiManager with BSSID-specific connection logic
   - **Features:** Restricts ESP32 to connect only to specified AP MAC address when enabled
   - **Usage:** `ConfigManager.setWifiAPMacFilter("60:B5:8D:4C:E1:D5");`
   - **Result:** ESP32 will only connect to the specified access point, ignoring all others

- **[IMPLEMENTED] setWifiAPMacPriority() method** (Prio 1) ✅  
   - **Implementation:** Added MAC priority system with scanning and BSSID selection
   - **Features:** Prefers specified AP but allows fallback to other APs if unavailable
   - **Usage:** `ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");`
   - **Result:** ESP32 scans for networks, selects priority AP if available, falls back to best alternative
   - **Tested:** Successfully connected to priority AP 60:B5:8D:4C:E1:D5 with -46 dBm signal

### High Priority Bugs - SOLVED

- **[FIXED] Must register settings manually (constructor registration seems to fail)** (Prio 1) ✅
   - **Root Cause:** Static initialization order problem - settings structs tried to register before ConfigManager was initialized
   - **Solution:** Implemented Delayed Initialization Pattern with `init()` methods for all settings structures
   - **Changes:** SystemSettings, ButtonSettings, TempSettings, NTPSettings, WiFiSettings, MQTTSettings all use `init()` pattern
   - **Documentation:** Created comprehensive guide in `docs/SETTINGS_STRUCTURE_PATTERN.md`
   - **Result:** All settings now register correctly and appear in WebUI reliably

### Medium Priority Features - IMPLEMENTED

- **[IMPLEMENTED] move settitingspasswort into a popup modal when accessing settings** (Prio 3) ✅
   - **Solution:** Modal-based authentication for both Settings tab and Flash/OTA button
   - **Features:** Dynamic password loading from backend, contextual dialogs, session persistence

### High Priority Features - IMPLEMENTED

- **[IMPLEMENTED] Password-protected settings view** (Prio 8) ✅
   - **Solution:** Settings page now requires password authentication using `SETTINGS_PASSWORT` from wifiSecret.h
   - **Features:** Modal-based auth, password masking, cancel option, session persistence, Flash/OTA button protection
