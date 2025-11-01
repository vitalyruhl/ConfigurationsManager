# Todo / Errors / Ideas

## Current Issues to Fix

### High Priority Bugs

- **[BUG] defineRuntimeIntSlider slider doesnt work** (Prio 1)
- **[BUG] defineRuntimeFloatSlider slider doesnt work** (Prio 1)
- **[BUG] bootloader_mmap- its with analog slider? or its general?** (Prio 1) 
    - E (39037) bootloader_mmap: tried to bootloader_mmap twice
    - E (39038) esp_image: bootloader_mmap(0x10020, 0x3530c) failed

- **[BUG] Must register settings manually (constructor registration seems to fail)** (Prio 1) 


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

- **[FEATURE] move settitingspasswort into a popup modal when accessing settings** (Prio 6)
- **[FEATURE] add a simply password encryption for "dump users", that not see clear passwords over http sending** (Prio 6)
- **[FEATURE] add HTTPS support** (Prio 10), because its not in core ESP32 WiFi lib yet.
