
# todo / errors / ideas

## errors / bugs

- on "DCM_ENABLE_RUNTIME_CHECKBOXES=0", the slider in settings are gone too. (bug!)
- on "DCM_ENABLE_STYLE_RULES=0", the style of Alarm is broken - no green on no alarm, alarm itself is red, but not blinking (bug!)
- remove CM_ENABLE_DYNAMIC_VISIBILITY in code
- remove CM_ALARM_GREEN_ON_FALSE in code
- remove CM_ENABLE_RUNTIME_CONTROLS in code
- Ota flash button: Password field is is a text field, not password field (bug!)
- on DCM_ENABLE_VERBOSE_LOGGING=1, and ota =0 -> we got spammed with verbose logging of ota deactive (bug!)

## ideas / todo


- make other componnents dependet of the flags (ota, websocked, actionbutton, alarms)
on?) script, that check the componnent on/of + compiling = success for all flags. (we add it then to test-engine if all works well)
- we have a precesion settings: {"group":"alarms","key":"dewpoint_risk","label":"Dewpoint Risk","precision":1,"isBool":true}  but the /runtime.json send "dew":13.5344, - its not rounded, i think we can reduce the size of the json if we use the precision for numeric values too.


