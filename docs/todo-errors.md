
# todo / errors / ideas


## Missing/Unused ConfigManager Functions in main.cpp

### Basic Management Functions (NOT USED in main.cpp):
- `findSetting(category, key)` - Find a specific setting by category and key
- `getSettingsCount()` - Get total number of registered settings
- `debugPrintSettings()` - Debug print all registered settings with details
- `applySetting(category, key, value)` - Apply setting to memory only (temporary)
- `updateSetting(category, key, value)` - Update setting and save to flash

### WiFi Management Functions (NOT USED):
- `getWiFiStatus()` - Check if WiFi is connected
- `reconnectWifi()` - Force WiFi reconnection

### Custom CSS Functions (PARTIALLY USED):
- ✅ `setCustomCss(css, len)` - USED in main.cpp
- `getCustomCss()` - Get custom CSS content
- `getCustomCssLen()` - Get custom CSS length

### System Control (NOT USED):
- `reboot()` - Restart the ESP32 system

### OTA Functions (PARTIALLY USED):
- ✅ `setupOTA(hostname, password)` - USED in main.cpp
- `handleOTA()` - Handle OTA updates (should be called in loop)
- `enableOTA(enabled)` - Enable/disable OTA functionality
- `isOTAInitialized()` - Check if OTA is initialized
- `stopOTA()` - Stop OTA (not implemented)

### Runtime Provider Functions (PARTIALLY USED):
- ✅ `addRuntimeProvider(provider)` - USED in main.cpp
- ✅ `addRuntimeProvider(name, fillFunc, order)` - USED in main.cpp

### Runtime Field Definition Functions (PARTIALLY USED):
- ✅ `defineRuntimeField(category, key, name, unit, min, max)` - AVAILABLE but not used in main.cpp
- ✅ `defineRuntimeButton()` - USED in main.cpp
- ✅ `defineRuntimeCheckbox()` - USED in main.cpp  
- ✅ `defineRuntimeStateButton()` - USED in main.cpp
- ✅ `defineRuntimeIntSlider()` - USED in main.cpp
- ✅ `defineRuntimeFloatSlider()` - USED in main.cpp
- ✅ `defineRuntimeBool()` - AVAILABLE but not used in main.cpp
- ✅ `defineRuntimeAlarm()` - AVAILABLE but not used in main.cpp
- `handleRuntimeAlarms()` - Handle runtime alarms (should be called periodically)

### WebSocket Functions (PARTIALLY USED):
- ✅ `enableWebSocketPush(intervalMs)` - USED in main.cpp
- ✅ `handleWebsocketPush()` - USED in main.cpp
- `disableWebSocketPush()` - Disable WebSocket push
- `setWebSocketInterval(intervalMs)` - Set WebSocket push interval
- `setPushOnConnect(enabled)` - Set whether to push on client connect
- `setCustomLivePayloadBuilder(function)` - Set custom payload builder

### Module Access Functions (PARTIALLY USED):
- ✅ `getWiFiManager()` - USED in main.cpp
- ✅ `getOTAManager()` - USED in main.cpp
- ✅ `getRuntimeManager()` - USED in main.cpp
- `getWebManager()` - Get web manager for advanced usage

### Static Functions (PARTIALLY USED):
- ✅ `ConfigManagerClass::setLogger(callback)` - USED in main.cpp
- `ConfigManagerClass::log_message(format, ...)` - Static logging function
- `triggerLoggerTest()` - Test the logger functionality

### System Provider Functions (PARTIALLY USED):
- ✅ `enableBuiltinSystemProvider()` - USED in main.cpp
- ✅ `updateLoopTiming()` - USED in main.cpp

## Functions that should be added to main.cpp for better functionality:

1. **In loop():**
   ```cpp
   ConfigManager.handleOTA(); // For OTA functionality
   ConfigManager.handleRuntimeAlarms(); // For alarm processing
   ```

2. **For debugging/monitoring:**
   ```cpp
   ConfigManager.debugPrintSettings(); // In setup() for debugging
   bool wifiStatus = ConfigManager.getWiFiStatus(); // For status monitoring
   ```

3. **For advanced runtime fields:**
   ```cpp
   ConfigManager.defineRuntimeField("sensors", "range", "Sensor Range", "V", 0.0, 5.0);
   ConfigManager.defineRuntimeBool("status", "connected", "Connection Status", false, 1);
   ConfigManager.defineRuntimeAlarm("alarms", "overheat", "Overheat Warning", []() { return temperature > 80; });
   ```

4. **For WebSocket customization:**
   ```cpp
   ConfigManager.setWebSocketInterval(1000); // Faster updates
   ConfigManager.setPushOnConnect(true); // Immediate data on connect
   ```
    


## errors / bugs

- [FIXED] ~~on DCM_ENABLE_VERBOSE_LOGGING=1, and ota =0 -> we got spammed with verbose logging of ota deactive~~
- [FIXED] ~~on "CM_ENABLE_RUNTIME_CHECKBOXES=0", the slider in settings are gone too.~~
- [FIXED] ~~void defineRuntimeStateButton, defineRuntimeBool, defineRuntimeAlarm, defineRuntimeFloatSlider not implemented~~
- [BUG] on "CM_ENABLE_STYLE_RULES=0", the style of Alarm is broken - no green on no alarm, alarm itself is red, but not blinking
- [BUG] there is somewhere a blocking call, that the webinterface is sometimes not reachable for some seconds (10-20s).
- [BUG] Ota flash button: Password field is is a text field, not password field
- [CRITICAL] Boolean settings save failure - fromJSON() returns false, settings don't persist
- [CRITICAL] Interactive controls missing - no toggles, buttons, sliders in GUI (only text inputs)
- [BUG] Firefox browser compatibility - freezes when accessing web interface (Chrome works)
- [BUG] Flash/OTA button not available in web interface
- [BUG] Password visibility - stored passwords not retrievable after reboot (only visible when changed)
- [BUG] showIf conditional display logic not working (Static IP fields always visible)
- [Refactoring] remove CM_ENABLE_DYNAMIC_VISIBILITY in code
- [Refactoring] remove CM_ALARM_GREEN_ON_FALSE in code
- [Refactoring] remove CM_ENABLE_RUNTIME_CONTROLS in code    
## ideas / todo


- [FEATURE REQUEST] Password-protected settings view to prevent unauthorized configuration changes
- make other componnents dependet of the flags (ota, websocked, actionbutton, alarms)
on?) script, that check the componnent on/of + compiling = success for all flags. (we add it then to test-engine if all works well)
- we have a precesion settings: {"group":"alarms","key":"dewpoint_risk","label":"Dewpoint Risk","precision":1,"isBool":true}  but the /runtime.json send "dew":13.5344, - its not rounded, i think we can reduce the size of the json if we use the precision for numeric values too.


