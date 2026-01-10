
/*
                                  __________________________
                                  | .--------------------. |
                                  | .   ~~~~~~~~~~~~~~   . |
                                  | .   ~~~~~~~~~~~~~~   . |
VP   ✅ ADC1 (RO)                | [ ]                 [x] | D23 <— Used for Relay
VN   ✅ ADC1 (RO)                | [ ]                 [ ] | D22 <— I2C_SCL (Optional Display)
D34  ✅ ADC1 (RO)                | [ ]                 [ ] | GPIO1 (TXD) <— UART TX (⚠️ avoid if possible)
D35  ✅ ADC1 (RO)                | [ ]                 [ ] | GPIO3 (RXD) <— UART RX (⚠️ avoid if possible)
D32  ✅ ADC1                     | [ ]                 [x] | D21 <— I2C_SDA (Optional Display)
D33  ✅ ADC1                     | [ ]                 [x] | D19 ❌ no ADC <— Used for will Shower Button
D25  ⚠️ ADC2                     | [ ]                 [ ] | D18 ❌ no ADC
D26  ⚠️ ADC2 DS18B20 Temp-Probe->| [x]                 [ ] | D5  ❌ no ADC
D27  ⚠️ ADC2                     | [ ]                 [ ] | D17 ❌ no ADC
D14  ⚠️ ADC2                     | [ ]                 [ ] | D16 ❌ no ADC
D12  ⚠️ ADC2 (boot pin)          | [ ]                 [ ] | D4  ⚠️ ADC2 (boot pin)
D13  ❌ no ADC (Btn-AP-Mode)     | [x]                 [x] | D15 ⚠️ ADC2 (boot pin - must be LOW on boot) (Used for Reset Button)
EN                                | [ ]                 [ ] | GND
VIN (5V!)                         | [ ]                 [ ] | 3V3
                                  |                         |
                                  |   [pwr-LED]  [D2-LED]   |
                                  |        _______          |
                                  |        |     |          |
                                  '--------|-----|---------'

Legend:
[x] in use    - Pin is already used in your project
✅ ADC1        - 12-bit ADC, usable even with WiFi
✅ ADC1 (RO)   - Input-only pins with ADC1 (GPIO36–39)
⚠️ ADC2        - 12-bit ADC, unusable when WiFi is active
❌ no ADC     - No analog capability
⚠️ Boot pin   - Must be LOW or unconnected at boot to avoid boot failure

Notes:
- GPIO0, GPIO2, GPIO12, GPIO15 are boot strapping pins — avoid pulling HIGH at boot.
- GPIO6–11 are used for internal flash – **never use**.
- GPIO1 (TX) and GPIO3 (RX) are used for serial output – use only if UART0 not needed.

- EN Turn to Low to reset the ESP32. On goint High, the ESP32 boots.
- VIN (5V!) is the power input pin, connect to 5V.
- VP (GPIO36) ADC1 (RO) No Pull-up/down possible.
- VN (GPIO39) ADC1 (RO) No Pull-up/down possible.

UART	TX	RX
UART0	GPIO1	GPIO3
UART1	GPIO10	GPIO9
UART2	GPIO17	GPIO16


*/


mklink /D "C:\Users\admin\Documents\privat\SolarInverterLimiter\.pio\libdeps\mgr\ESP32 Configuration Manager" "C:\Users\admin\Documents\privat\ConfigurationsManager"

New-Item -ItemType SymbolicLink -Path "C:\Users\admin\Documents\privat\SolarInverterLimiter\.pio\libdeps\mgr\ESP32 Configuration Manager" -Target "C:\Users\admin\Documents\privat\ConfigurationsManager"

Remove-Item -Path "C:\Users\admin\Documents\privat\SolarInverterLimiter\.pio\libdeps\mgr\ESP32 Configuration Manager" -Force