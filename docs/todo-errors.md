# Todo / Errors / Ideas

## Current Issues to Fix

### High Priority Bugs

- **[BUG] defineRuntimeIntSlider slider doesnt work** (Prio 1)
- **[BUG] defineRuntimeFloatSlider slider doesnt work** (Prio 1)
- **[BUG] bootloader_mmap- its with analog slider? or its general?** (Prio 1) 
    - E (39037) bootloader_mmap: tried to bootloader_mmap twice
    - E (39038) esp_image: bootloader_mmap(0x10020, 0x3530c) failed

- **[FIXED] Must register settings manually (constructor registration seems to fail)** (Prio 1) ✅
   - **Root Cause:** Static initialization order problem - settings structs tried to register before ConfigManager was initialized
   - **Solution:** Implemented Delayed Initialization Pattern with `init()` methods for all settings structures
   - **Changes:** SystemSettings, ButtonSettings, TempSettings, NTPSettings, WiFiSettings, MQTTSettings all use `init()` pattern
   - **Documentation:** Created comprehensive guide in `docs/SETTINGS_STRUCTURE_PATTERN.md`
   - **Result:** All settings now register correctly and appear in WebUI reliably 


## Feature Requests / Ideas

- **[IMPLEMENTED] Password-protected settings view** (Prio 8) ✅
   - ~~Prevent unauthorized configuration changes~~
   - ~~Add authentication layer for sensitive settings~~
   - **Solution:** Settings page now requires password authentication (default: "cf")
   - **Features:** Modal-based auth, password masking, cancel option, session persistence

- **[FEATURE] Automated component testing** (Prio 10)
   - Create script that checks component on/off flags
   - Ensure compilation succeeds for all flag combinations
   - Integrate into test engine for comprehensive validation

- **[IMPLEMENTED] move settitingspasswort into a popup modal when accessing settings** (Prio 3) ✅
   - **Solution:** Modal-based authentication for both Settings tab and Flash/OTA button
   - **Features:** Dynamic password loading from backend, contextual dialogs, session persistence
- **[FEATURE] add a simply password encryption for "dump users", that not see clear passwords over http sending** (Prio 2)
- **[FEATURE] add HTTPS support** (Prio 10), because its not in core ESP32 WiFi lib yet.
