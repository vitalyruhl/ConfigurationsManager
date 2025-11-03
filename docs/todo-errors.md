# Todo / Errors / Ideas

## Current Issues to Fix


### High Priority Bugs (Prio 1)

- FIXED: Flash password flow and modal behavior
   - Flash now asks for the OTA password on button press in a modal popup (single prompt), then opens the file chooser and uploads without asking again.
   - Flash is no longer gated by the Settings authentication modal.
   - The Settings password prompt is also a true modal overlay (centered, blocks background).

- FIXED: Category header shows truncated technical name instead of pretty title
   - Added support for categoryPretty on the backend and frontend.
   - Example category "VeryLongCategoryName" now displays as "category Correction long".
   - Usage:
     ```cpp
     Config<float> VeryLongCategoryName(ConfigOptions<float>{
         .key = "VlongC",
         .name = "category Correction long",
         .category = "VeryLongCategoryName",
         .defaultValue = 0.1f,
         .categoryPretty = "category Correction long"});
     ```
   - Category cards prefer the pretty name if provided; falls back to the raw category otherwise.
    


### Medium Priority Features (Prio 5)

- if i go to settings, i get 
   [WebServer] config.json sent successfully (6218 bytes)
   [ConfigManager] [WS] Client connect 1
   [ConfigManager] [WS] Client disconnect 1
   [ConfigManager] [WS] Client connect 2
   [ConfigManager] [WS] Client disconnect 2
   [ConfigManager] [WS] Client connect 3 ... etc secondly, but not allways....



### Low Priority Features

- **[FEATURE] Automated component testing** (Prio 10)
   - Create script that checks component on/off flags
   - Ensure compilation succeeds for all flag combinations
   - Integrate into test engine for comprehensive validation

- **[FEATURE] add HTTPS support, because its not in core ESP32 WiFi lib yet.** (Prio: not yet, wait for updates)

- if there are more cards, the card brocken down is under the longest card from above. It looks not fine -> remake the grid to be more flexible (Prio 10)