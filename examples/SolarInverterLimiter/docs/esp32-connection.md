# ESP32 Connection overwiew (ESP32-WROOM-32 - development board)

This document is a **project-specific** GPIO reference for an ESP32-WROOM-32 board.

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
                                                           EN  | [x]                        [x] | GPIO23 (DO/PWM)                                [Cooling-Fan-Relay]
                                                               |                                |
[free]         {RO/no-Pull!}       (DI/ADC1)          PIO36/VP | [ ]                        [x] | GGPIO22 (DI/DO)                                [SCL for I2C]
                                                               |                                |
[free]         {RO/no-Pull!}       (DI/ADC1)         GPIO39/VN | [ ]                        [ ] | GPIO1  (DO)               {UART1 TX only}      [free]
                                                               |                                |
[Heater-Relay] {RO/no-Pull!}       (DI/ADC1)            GPIO34 | [x]                        [ ] | GPIO3  (DI)               {UART1 RX only}      [free]
                                                               |                                |
[free]         {RO/no-Pull!}       (DI/ADC1)            GPIO35 | [ ]                        [x] | GPIO21 (DI/DO)                                 [SDA for I2C]
                                                               |                                |
[free]                             (DI/DO/ADC1)         GPIO32 | [ ]                        [x] | GPIO19 (DI/DO/PWM)                             [TX to RS485-Module]
                                                               |                                |
[free]                             (DI/DO/ADC1)         GPIO33 | [ ]                        [x] | GPIO18 (DI/DO/PWM)         {ADC2!}             [RX from RS485-Module - not used in code!]
                                                               |                                |
[free]         {ADC2!}             (DI/DO/DAC1/PWM)     GPIO25 | [ ]                        [ ] | GPIO5  (DI/DO/PWM)         {ADC2!}{BOOT}       [free]
                                                               |                                |
[free]         {ADC2!}             (DI/DO/DAC2/PWM)     GPIO26 | [ ]                        [ ] | GPIO17 (DO/PWM)            {ADC2!}             [free]
                                                               |                                |
[free]         {ADC2!}             (DO/PWM)             GPIO27 | [ ]                        [ ] | GPIO16 (DI/DO/PWM)         {ADC2!}             [free]
                                                               |                                |
[Reset-Button] {ADC2!}             (DO/PWM)             GPIO14 | [x]                        [ ] | GPIO4  (DO/PWM)            {BOOT}              [free]
                                                               |                                |
[free]         {BOOT/ADC2!}        (DI/DO/ADC2!/PWM)    GPIO12 | [ ]                        [ ] | GPIO2  (DO)                {Build-In LED only} [free]
                                                               |                                |
[AP-Mode-Button]                   (DI/DO)              GPIO13 | [x]                        [ ] | GPIO15 (DI/DO)             {LOW on BOOT!}      [free]
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
