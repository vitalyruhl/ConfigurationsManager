
# todo / errors / ideas

- on "DCM_ENABLE_RUNTIME_CHECKBOXES=0", the slider in settings are gone too. (bug!)


## errors / bugs


## ideas / todo
- make other componnents dependet of the flags (ota, websocked, actionbutton, alarms)
on?) script, that check the componnent on/of + compiling = success for all flags. (we add it then to test-engine if all works well)
- we have a precesion settings: ``` txt
        [9	
        group	"sensors"
        key	"dew"
        label	"Dewpoint"
        unit	"°C"
        precision	1
        order	12]
        but the /runtime.json send : dew	14.57077 - its not rounded. please do it...
        ```


