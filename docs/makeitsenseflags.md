# Make It Sense Flags? (Build-Time)

- Built with CM_EMBED_WEBUI=0 (external UI mode): Flash: 986,885 bytes (75.3%)
- Built with CM_EMBED_WEBUI=1 (embedded UI mode): Flash:  1,097,177 bytes (83.7%) --> Delta: ~110 KB larger than GUI=0
  - add gzipped HTML/CSS/JS bundle (~90 KB), now: 78.3% (used 1,025,957 bytes from 1,310,720 bytes)

>So GUI on costs ~39 KB now, down from ~110 KB. - Make it sense to deaktivate Feautures?


- DCM_ENABLE_WS_PUSH=0 (WebSocket push disabled): Flash: 77.4% (used 1,014,121 bytes from 1,310,720 bytes) --> Delta: ~11 KB smaller 

- DCM_ENABLE_SYSTEM_PROVIDER=0 (system card disabled): 78.0% (used 1,023,005 bytes from 1,310,720 bytes) --> Delta: ~2 KB smaller

- DCM_ENABLE_OTA=0 (OTA disabled): 73.4% (used 962645 bytes from 1310720 bytes) --> Delta: ~63 KB smaller

- DCM_ENABLE_RUNTIME_BUTTONS=0 (Runtime Buttons disabled): 78.1% (used 1023737 bytes from 1310720 bytes) --> Delta: ~650 bytes smaller

- DCM_ENABLE_RUNTIME_CHECKBOXES=0 (Runtime Checkboxes disabled): 78.1% (used 1023233 bytes from 1310720 bytes) --> Delta: ~500 bytes smaller

- DCM_ENABLE_RUNTIME_STATE_BUTTONS=0 (Runtime State Buttons disabled): 78.1% (used 1023137 bytes from 1310720 bytes) --> Delta: ~600 bytes smaller

- **DCM_ENABLE_RUNTIME_ANALOG_SLIDERS**=0 (Runtime Analog Sliders disabled): 77.7% (used 1017957 bytes from 1310720 bytes) --> Delta: ~**5200** bytes smaller