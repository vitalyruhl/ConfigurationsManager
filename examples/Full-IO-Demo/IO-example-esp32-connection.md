# IO Example ESP32 Connection (ESP32-WROOM-32)

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

## Orientation

Orientation:
- Antenna: TOP
- USB connector: BOTTOM

## Legend

Legend:
- [x] = used
- [ ] = free
- [Current Use] = current project usage
- (DI/DO/ADC1/ADC2/DAC1/DAC2/PWM) = capabilities
- {…} = constraints / warnings
- (RO) = read-only input (no output, no pullups)

## Pin map (ASCII)

```text

                                              _________________________________
                                              | .---------------------------.  |
                                              | .   ~~~~~~ Antenna ~~~~~~   .  |
                                              | .   ~~~~~~~~~~~~~~~~~~~~~   .  |
                                          EN  | .   ~~~~~~~~~~~~~~~~~~~~~   .  |
                                              | .   ~~~~~~~~~~~~~~~~~~~~~   .  |
[LDR]          (DI/ADC1/RO) {RO}     PIO36/VP | [x]                        [x] | GPIO23 (DO/PWM)        [Fan Relay]
                                              |                                |
[LDR]          (DI/ADC1/RO) {RO}    GPIO39/VN | [x]                        [x] | GPIO22 (DI/DO)         [I2C SCL]
                                              |                                |
[LDR]          (DI/ADC1/RO) {RO}       GPIO34 | [x]                        [ ] | GPIO1  (DO)            [free]
                                              |                                |
[LDR]          (DI/ADC1/RO) {RO}       GPIO35 | [x]                        [ ] | GPIO3  (DI)            [free]
                                              |                                |
[DS18B20]      (DI/DO/ADC1)            GPIO32 | [x]                        [x] | GPIO21 (DI/DO)         [I2C SDA]
                                              |                                |
[Test Btn]     (DI/DO/ADC1)            GPIO33 | [x]                        [ ] | GPIO19 (DI/DO/PWM)     [PWM test]
                                              |                                |
[Analog 1]     (DI/DO/DAC1/ADC2/PWM)   GPIO25 | [x]                        [ ] | GPIO18 (DI/DO/PWM)     [PWM test]           {ADC2!}
                                              |                                |
[Analog 2]     (DI/DO/DAC2/ADC2/PWM)   GPIO26 | [x]                        [ ] | GPIO5  (DI/DO/PWM)     [free]               {ADC2!}{BOOT}
                                              |                                |
[Hold Relay]   (DO/ADC2/PWM)           GPIO27 | [x]                        [ ] | GPIO17 (DO/PWM)        [UART2 TX]           {ADC2!}
                                              |                                |
[Reset-Button] (DO/ADC2/PWM)           GPIO14 | [x]                        [ ] | GPIO16 (DI/DO/PWM)     [UART2 RX]           {ADC2!}
                                              |                                |
[free]         (DI/DO/ADC2/PWM)        GPIO12 | [ ]                        [ ] | GPIO4  (DO/PWM)        [Heater-Relay]       {BOOT}
                                              |                                |
[AP Btn]       (DI/DO)                 GPIO13 | [x]                        [ ] | GPIO15 (DI/DO)         [Factory]            {BOOT}
                                              |                                |
                                              |      [pwr-LED]  [GPIO2-LED]    |
                                              |                                |
                                              |            _______             |
                                              |            |     |             |
                                              '------------|-----|------------'


```
## Pin capability table

| GPIO | [x] | Current Use            | Capabilities                    | Constraints |
|-----:|:--:|------------------------|---------------------------------|-------------|
| 36   | x  | LDR Input              | (DI/ADC1/RO)                    | no pullups |
| 39   | x  | LDR Input              | (DI/ADC1/RO)                    | no pullups |
| 34   | x  | LDR Input              | (DI/ADC1/RO)                    | no pullups |
| 35   | x  | LDR Input              | (DI/ADC1/RO)                    | no pullups |
| 32   | x  | DS18B20                | (DI/DO/ADC1/PWM)                | — |
| 33   | x  | Test Button            | (DI/DO/ADC1/PWM)                | — |
| 25   | x  | Analog Out 1           | (DI/DO/ADC2/DAC1/PWM)           | ADC2 unusable on WiFi |
| 26   | x  | Analog Out 2           | (DI/DO/ADC2/DAC2/PWM)           | ADC2 unusable on WiFi |
| 27   | x  | Hold Relay             | (DO/ADC2/PWM)                   | ADC2 unusable on WiFi |
| 14   | x  | Heater Relay           | (DO/ADC2/PWM)                   | ADC2 unusable on WiFi |
| 12   |    | free                   | (DI/DO/ADC2/PWM)                | BOOT strap |
| 13   | x  | AP Mode Button         | (DI/DO)                         | — |
| 23   | x  | Fan Relay              | (DO/PWM)                        | — |
| 22   | x  | I2C SCL                | (DI/DO)                         | I2C pullups |
| 21   | x  | I2C SDA                | (DI/DO)                         | I2C pullups |
| 19   |    | planned: PWM test      | (DI/DO/PWM)                     | — |
| 18   |    | planned: PWM test      | (DI/DO/PWM)                     | — |
| 5    |    | free                   | (DI/DO/PWM)                     | BOOT strap |
| 17   |    | UART2 TX (planned)     | (DO/PWM)                        | — |
| 16   |    | UART2 RX (planned)     | (DI/DO/PWM)                     | — |
| 4    |    | free                   | (DO/PWM)                        | BOOT strap |
| 15   | x  | Factory Reset Button   | (DI/DO)                         | BOOT: must be LOW |
| 2    | x  | On-board LED           | (DO)                            | affects boot visuals |

---

## Pin swap hints

- Prefer **ADC1 pins (GPIO32–39)** for analog inputs when WiFi or BT is enabled.
- **ADC2 pins cannot be reliably used for analogRead while WiFi/BT is active**.
- BOOT strap pins (GPIO0, 2, 4, 5, 12, 15) must not be forced into unsafe levels at boot.
- Use **PWM + RC filter** on free GPIOs if more “analog outputs” are required instead of DAC.
- GPIO34–39 are **(RO)**: input only, no output, no internal pullups.

---
