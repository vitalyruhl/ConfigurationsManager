
/*
                                  __________________________
                                  | .--------------------. |
                                  | .   ~~~~~~~~~~~~~~   . |
                                  | .   ~~~~~~~~~~~~~~   . |
VP   [OK] ADC1 (RO)              | [ ]                 [x] | D23 <— Used for Fan-Relay
VN   [OK] ADC1 (RO)              | [ ]                 [x] | D22 <— I2C_SCL
D34  [OK] ADC1 (RO)              | [ ]                 [ ] | GPIO1 (TXD) <— UART TX ([WARNING] avoid if possible)
D35  [OK] ADC1 (RO)              | [ ]                 [ ] | GPIO3 (RXD) <— UART RX ([WARNING] avoid if possible)
D32  [OK] ADC1                   | [ ]                 [x] | D21 <— I2C_SDA
D33  [OK] ADC1                   | [ ]                 [x] | D19 [INFO] no ADC (Tx Pin for Serial2 = RS232-to-RS485)
D25  [WARNING] ADC2              | [ ]                 [x] | D18 [INFO] no ADC (Rx Pin for Serial2 = RS232-to-RS485)
D26  [WARNING] ADC2              | [ ]                 [ ] | D5  [INFO] no ADC
D27  [WARNING] ADC2              | [ ]                 [ ] | D17 [INFO] no ADC
D14  [WARNING] ADC2              | [ ]                 [ ] | D16 [INFO] no ADC
D12  [WARNING] ADC2 (boot pin)   | [ ]                 [ ] | D4  [WARNING] ADC2 (boot pin)
D13  [INFO] no ADC (Btn-AP-Mode) | [x]                 [x] | D15 [WARNING] ADC2 (boot pin - must be LOW on boot) (Used for Reset Button)
EN                                | [ ]                 [ ] | GND
VIN (5V!)                         | [ ]                 [ ] | 3V3
                                  |                         |
                                  |   [pwr-LED]  [D2-LED]   |
                                  |        _______          |
                                  |        |     |          |
                                  '--------|-----|---------'

Legend:
[x] in use    - Pin is already used in your project
[OK] ADC1        - 12-bit ADC, usable even with WiFi
[OK] ADC1 (RO)   - Input-only pins with ADC1 (GPIO36–39)
[WARNING] ADC2   - 12-bit ADC, unusable when WiFi is active
[INFO] no ADC    - No analog capability
[WARNING] Boot pin - Must be LOW or unconnected at boot to avoid boot failure

Notes:
- GPIO0, GPIO2, GPIO12, GPIO15 are boot strapping pins — avoid pulling HIGH at boot.
- GPIO6–11 are used for internal flash – **never use**.
- GPIO1 (TX) and GPIO3 (RX) are used for serial output – use only if UART0 not needed.

- EN: Pull LOW to reset the ESP32. When released HIGH, the ESP32 boots.
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