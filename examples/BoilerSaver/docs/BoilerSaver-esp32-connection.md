# ESP32 wiring and operational mapping

## Application profiles

The normal application defaults use DS18B20 GPIO26, I2C SDA21/SCL22,
boiler relay23, status LED2, reset input14, AP input13 and shower input19.
Stored settings can override exposed sensor/IO pins. The normal OLED reset is4.

The `testesp` profile is for the operator-confirmed COM8 station only:

| Function | Station GPIO |
| --- | --- |
| DS18B20 | 18 |
| I2C SDA / SCL | 21 / 22 |
| OLED reset | -1 (not connected) |
| Unloaded dummy boiler output | 25 |
| Status LED | 26 |
| Reset / shower / set inputs | 14 / 33 / 4 |
| Reserved, unregistered relays | 19 / 27 / 23 |

Test startup applies this mapping, DHCP and an isolated MQTT identity in RAM.
Heating is disabled and the lower threshold is -1000 C. Never connect the dummy
output to heating equipment or upload this mapping to the production unit.

## Board capability reference

The diagram below is an older board reference, not a source for profile wiring.
Use the explicit operational mapping above and verify the actual wiring.

## Overview

- Helps picking safe GPIOs for digital inputs/outputs, ADC and DAC
- Highlights ESP32 constraints (ADC2 with WiFi, BOOT strap pins, RO-only pins)

## Board info

Board:
- Module: ESP32-WROOM-32
- Chip: ESP32-D0WD-V3 (rev 3)
- Cores: 2 @ 240 MHz
- Flash: 4 MB
- PSRAM: none
- On-board LED: GPIO2
- Power LED: fixed (no GPIO)

## Legend

Legend:
- [x] = used = current project usage
- [ ] = free
- (DI/DO/ADC1/ADC2/DAC1/DAC2/PWM) = capabilities (ADC2 unusable with WiFi/BT!)
- {…} = constraints / warnings
- (RO) = read-only input (no output, no pullups)
- [current usage description] = what is connected in this project

## Pin map (ASCII)

```text

                                                               __________________________________
                                                               | .---------------------------.  |
                                                               | .   ~~~~~~ Antenna ~~~~~~   .  |
                                                               | .   ~~~~~~~~~~~~~~~~~~~~~   .  |
                                                               | .   ~~~~~~~~~~~~~~~~~~~~~   .  |
                                                           EN  | [x]                        [x] | GPIO23 (DO/PWM)                                [Relay]
                                                               |                                |
[free]         {RO/no-Pull!}       (DI/ADC1)          PIO36/VP | [ ]                        [x] | GGPIO22 (DI/DO)                                [I2C SCL]
                                                               |                                |
[free]         {RO/no-Pull!}       (DI/ADC1)         GPIO39/VN | [ ]                        [ ] | GPIO1  (DO)               {UART1 TX only}      [free]
                                                               |                                |
[free]         {RO/no-Pull!}       (DI/ADC1)            GPIO34 | [ ]                        [ ] | GPIO3  (DI)               {UART1 RX only}      [free]
                                                               |                                |
[free]         {RO/no-Pull!}       (DI/ADC1)            GPIO35 | [ ]                        [x] | GPIO21 (DI/DO)                                 [I2C SDA]
                                                               |                                |
[free]                             (DI/DO/ADC1)         GPIO32 | [ ]                        [x] | GPIO19 (DI/DO/PWM)                             [Will shower-Button]
                                                               |                                |
[free]                             (DI/DO/ADC1)         GPIO33 | [ ]                        [x] | GPIO18 (DI/DO/PWM)         {ADC2!}             [DS18B20 (3v3-->4,7k-->yellow)-->pin]
                                                               |                                |
[free]         {ADC2!}             (DI/DO/DAC1/PWM)     GPIO25 | [ ]                        [ ] | GPIO5  (DI/DO/PWM)         {ADC2!}{BOOT}       [free]
                                                               |                                |
[free]         {ADC2!}             (DI/DO/DAC2/PWM)     GPIO26 | [ ]                        [ ] | GPIO17 (DO/PWM)            {ADC2!}             [free]
                                                               |                                |
[free]         {ADC2!}             (DO/PWM)             GPIO27 | [ ]                        [ ] | GPIO16 (DI/DO/PWM)         {ADC2!}             [free]
                                                               |                                |
[Reset-Button] {ADC2!}             (DO/PWM)             GPIO14 | [x]                        [ ] | GPIO4  (DO/PWM)            {BOOT}              [free]
                                                               |                                |
[free]         {BOOT/ADC2!}        (DI/DO/ADC2!/PWM)    GPIO12 | [ ]                        [x] | GPIO2  (DO)                {Build-In LED only}
                                                               |                                |
[AP Btn]                           (DI/DO)              GPIO13 | [x]                        [ ] | GPIO15 (DI/DO)             {LOW on BOOT!}      [free]
                                                               |                                |
                                                          GND  | [x]  [pwr-LED] [GPIO2-LED] [x] | GND
                                                               |                                |
                                                          5V   | [ ]                        [x] | 3,3V
                                                               |            _______             |
                                                               |  [EN/RST]  |     |   [Boot]    |
                                                               |            |     |             |
                                                               '------------|-----|------------'


```
## Pin capability table

| GPIO | Capabilities                    | Constraints |
|-----:|---------------------------------|-------------|
| 36   | (DI/ADC1/RO)                    | no pullups |
| 39   | (DI/ADC1/RO)                    | no pullups |
| 34   | (DI/ADC1/RO)                    | no pullups |
| 35   | (DI/ADC1/RO)                    | no pullups |
| 32   | (DI/DO/ADC1/PWM)                | — |
| 33   | (DI/DO/ADC1/PWM)                | — |
| 25   | (DI/DO/ADC2/DAC1/PWM)           | ADC2 unusable on WiFi |
| 26   | (DI/DO/ADC2/DAC2/PWM)           | ADC2 unusable on WiFi |
| 27   | (DO/ADC2/PWM)                   | ADC2 unusable on WiFi |
| 14   | (DO/ADC2/PWM)                   | ADC2 unusable on WiFi |
| 12   | (DI/DO/ADC2/PWM)                | BOOT strap |
| 13   | (DI/DO)                         | — |
| 23   | (DO/PWM)                        | — |
| 22   | (DI/DO)                         | I2C pullups |
| 21   | (DI/DO)                         | I2C pullups |
| 19   | (DI/DO/PWM)                     | — |
| 18   | (DI/DO/PWM)                     | — |
| 5    | (DI/DO/PWM)                     | BOOT strap |
| 17   | (DO/PWM)                        | — |
| 16   | (DI/DO/PWM)                     | — |
| 4    | (DO/PWM)                        | BOOT strap |
| 15   | (DI/DO)                         | BOOT: must be LOW |
| 2    | (DO)                            | affects boot visuals |

---

## Pin swap hints

- Prefer **ADC1 pins (GPIO32–39)** for analog inputs when WiFi or BT is enabled.
- **ADC2 pins cannot be reliably used for analogRead while WiFi/BT is active**.
- BOOT strap pins (GPIO0, 2, 4, 5, 12, 15) must not be forced into unsafe levels at boot.
- Use **PWM + RC filter** on free GPIOs if more “analog outputs” are required instead of DAC.
- GPIO34–39 are **(RO)**: input only, no output, no internal pullups.

---

## Validation scope

The standalone application snapshot 4.1.2 was observed on its operator-confirmed
COM8 station for five minutes without an unexpected reset or panic. One DS18B20
and the OLED I2C acknowledgement/init were verified; the unloaded output stayed
off. Five HTTP timeouts occurred, so the strict HTTP gate did not pass. A separate
bounded Live WebSocket observation succeeded. This packaged example itself has
not been uploaded or hardware-tested.

The 360-second display-off callback was outside the five-minute window. Physical
button actuation, visual OLED output, sensor calibration, production behavior and
long-term stability were not validated. The historical production panic remains
unresolved. Use the standalone project for maintained application context;
private raw device logs and investigation records are not shipped here.
