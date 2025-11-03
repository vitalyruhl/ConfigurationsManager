# Todo / Errors / Ideas

## Current Issues to Fix


### High Priority Bugs

- FIXED: Flash password flow and modal behavior
   - Flash now asks for the OTA password on button press in a modal popup (single prompt), then opens the file chooser and uploads without asking again.
   - Flash is no longer gated by the Settings authentication modal.
   - The Settings password prompt is also a true modal overlay (centered, blocks background).

-  on 
   - ```Config<float> VeryLongCategoryName(ConfigOptions<float>{
    .key = "VlongC",
    .name = "category Correction long",
    .category = "VeryLongCategoryName",
    .defaultValue = 0.1f});```
   - the category is shown as "VeryLongCate...", not as "category Correction long"
   - but Example Settings and Dynamic visibility example work fine.
    


### Medium Priority Features

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

- **[FEATURE] add HTTPS support, because its not in core ESP32 WiFi lib yet.** (Prio 10)

