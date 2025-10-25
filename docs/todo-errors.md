
# todo / errors / ideas

## errors / bugs

- [FIXED] ~~on DCM_ENABLE_VERBOSE_LOGGING=1, and ota =0 -> we got spammed with verbose logging of ota deactive~~
- [FIXED] ~~on "CM_ENABLE_RUNTIME_CHECKBOXES=0", the slider in settings are gone too.~~
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


 void defineRuntimeStateButton(const String &category, const String &key, const String &name, std::function<bool()> getter, std::function<void(bool)> setter)
    {
        // Note: State buttons not implemented in new RuntimeManager
        CM_LOG("[W] defineRuntimeStateButton not implemented in modular RuntimeManager");
    }

    void defineRuntimeBool(const String &category, const String &key, const String &name, bool defaultValue, int order = 0)
    {
        // Note: Use addRuntimeMeta directly instead
        CM_LOG("[W] defineRuntimeBool not implemented, use addRuntimeMeta directly");
    }

    void defineRuntimeAlarm(const String &category, const String &key, const String &name, std::function<bool()> condition, std::function<void()> onTrigger = nullptr, std::function<void()> onClear = nullptr)
    {
        // Note: Use addRuntimeAlarm directly instead
        CM_LOG("[W] defineRuntimeAlarm not implemented, use addRuntimeAlarm directly");
    }

    void defineRuntimeFloatSlider(const String &category, const String &key, const String &name, float min, float max, float &variable, int precision = 1, std::function<void()> onChange = nullptr)
    {
        // Note: Float sliders not implemented in new RuntimeManager
        CM_LOG("[W] defineRuntimeFloatSlider not implemented in modular RuntimeManager");
    }

    
## ideas / todo

- [FEATURE REQUEST] Password-protected settings view to prevent unauthorized configuration changes
- make other componnents dependet of the flags (ota, websocked, actionbutton, alarms)
on?) script, that check the componnent on/of + compiling = success for all flags. (we add it then to test-engine if all works well)
- we have a precesion settings: {"group":"alarms","key":"dewpoint_risk","label":"Dewpoint Risk","precision":1,"isBool":true}  but the /runtime.json send "dew":13.5344, - its not rounded, i think we can reduce the size of the json if we use the precision for numeric values too.


