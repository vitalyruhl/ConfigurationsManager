# ESP32 Connection overwiew (ESP32-WROOM-32 - development board)

This document is a **project-specific** GPIO reference for an ESP32-WROOM-32 board.

> more Info about ESP32: [ESPConnect](https://thelastoutpostworkshop.github.io/ESPConnect/) 
> note : only on cromium based browsers! 

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
                                                           EN  | [x]                        [ ] | GPIO23 (DO/PWM)                                [free]
                                                               |                                |
[free]         {RO/no-Pull!}       (DI/ADC1)          PIO36/VP | [ ]                        [ ] | GGPIO22 (DI/DO)                                [free]
                                                               |                                |
[free]         {RO/no-Pull!}       (DI/ADC1)         GPIO39/VN | [ ]                        [ ] | GPIO1  (DO)               {UART1 TX only}      [free]
                                                               |                                |
[free]         {RO/no-Pull!}       (DI/ADC1)            GPIO34 | [ ]                        [ ] | GPIO3  (DI)               {UART1 RX only}      [free]
                                                               |                                |
[free]         {RO/no-Pull!}       (DI/ADC1)            GPIO35 | [ ]                        [ ] | GPIO21 (DI/DO)                                 [free]
                                                               |                                |
[free]                             (DI/DO/ADC1)         GPIO32 | [ ]                        [ ] | GPIO19 (DI/DO/PWM)                             [free]
                                                               |                                |
[free]                             (DI/DO/ADC1)         GPIO33 | [ ]                        [ ] | GPIO18 (DI/DO/PWM)         {ADC2!}             [free]
                                                               |                                |
[free]         {ADC2!}             (DI/DO/DAC1/PWM)     GPIO25 | [ ]                        [ ] | GPIO5  (DI/DO/PWM)         {ADC2!}{BOOT}       [free]
                                                               |                                |
[free]         {ADC2!}             (DI/DO/DAC2/PWM)     GPIO26 | [ ]                        [ ] | GPIO17 (DO/PWM)            {ADC2!}             [free]
                                                               |                                |
[free]         {ADC2!}             (DO/PWM)             GPIO27 | [ ]                        [ ] | GPIO16 (DI/DO/PWM)         {ADC2!}             [free]
                                                               |                                |
[free]         {ADC2!}             (DO/PWM)             GPIO14 | [ ]                        [ ] | GPIO4  (DO/PWM)            {BOOT}              [free]
                                                               |                                |
[free]         {BOOT/ADC2!}        (DI/DO/ADC2!/PWM)    GPIO12 | [ ]                        [ ] | GPIO2  (DO)                {Build-In LED only}
                                                               |                                |
[free]                             (DI/DO)              GPIO13 | [ ]                        [ ] | GPIO15 (DI/DO)             {LOW on BOOT!}      [free]
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

## Features

### Wi-Fi

- 802.11b/g/n
- 802.11n (2.4 GHz), up to 150 Mbps
- WMM
- TX/RX A-MPDU, RX A-MSDU
- Immediate Block ACK
- Defragmentation
- Automatic Beacon monitoring (hardware TSF)
- Four virtual Wi-Fi interfaces
- Simultaneous support for Infrastructure Station, SoftAP, and Promiscuous modes
Note that when ESP32 is in Station mode, performing a scan, the SoftAP channel will be changed.
- Antenna diversity

### Bluetooth®

- Compliant with Bluetooth v4.2 BR/EDR and Bluetooth LE specifications
- Class-1, class-2 and class-3 transmitter without external power amplifier
- Enhanced Power Control
- +9 dBm transmitting power
- NZIF receiver with –94 dBm Bluetooth LE sensitivity
- Adaptive Frequency Hopping (AFH)
- Standard HCI based on SDIO/SPI/UART
- High-speed UART HCI, up to 4 Mbps
- Bluetooth 4.2 BR/EDR and Bluetooth LE dual mode controller
- Synchronous Connection-Oriented/Extended (SCO/eSCO)
- CVSD and SBC for audio codec
- Bluetooth Piconet and Scatternet
- Multi-connections in Classic Bluetooth and Bluetooth LE
- Simultaneous advertising and scanning

### CPU and Memory

- Xtensa® single-/dual-core 32-bit LX6 microprocessor(s)
- CoreMark® score:
  - 1 core at 240 MHz: 539.98 CoreMark; 2.25 CoreMark/MHz
Espressif Systems 3
Submit Documentation Feedback
ESP32 Series Datasheet v5.2
  - 2 cores at 240 MHz: 1079.96 CoreMark; 4.50 CoreMark/MHz
- 448 KB ROM
- 520 KB SRAM
- 16 KB SRAM in RTC
- QSPI supports multiple flash/SRAM chips

### Clocks and Timers

- Internal 8 MHz oscillator with calibration
- Internal RC oscillator with calibration
- External 2 MHz ~ 60 MHz crystal oscillator (40 MHz only for Wi-Fi/Bluetooth functionality)
- External 32 kHz crystal oscillator for RTC with calibration
- Two timer groups, including 2 × 64-bit timers and 1 × main watchdog in each group
- One RTC timer
- RTC watchdog

### Advanced Peripheral Interfaces

- 34 programmable GPIOs
  - Five strapping GPIOs
  - Six input-only GPIOs
  - Six GPIOs needed for in-package flash (ESP32-U4WDH) and in-package PSRAM
(ESP32-D0WDRH2-V3)
- 12-bit SAR ADC up to 18 channels
- Two 8-bit DAC
- 10 touch sensors
- Four SPI interfaces
- Two I2S interfaces
- Two I2C interfaces
- Three UART interfaces
- One host (SD/eMMC/SDIO)
- One slave (SDIO/SPI)
- Pulse count controller
- Ethernet MAC interface with dedicated DMA and IEEE 1588 support
- TWAI®, compatible with ISO 11898-1 (CAN Specification 2.0)
- RMT (TX/RX)
- Motor PWM
- LED PWM up to 16 channels

###Power Management

- Fine-resolution power control through a selection of clock frequency, duty cycle, Wi-Fi operating modes,
and individual power control of internal components
- Five power modes designed for typical scenarios: Active, Modem-sleep, Light-sleep, Deep-sleep,
Hibernation
- Power consumption in Deep-sleep mode is 10 µA
- Ultra-Low-Power (ULP) coprocessors
- RTC memory remains powered on in Deep-sleep mode

### Security

- Secure boot
- Flash encryption
- 1024-bit OTP, up to 768-bit for customers
- Cryptographic hardware acceleration:
  - AES
  - Hash (SHA-2)
  - RSA
  - Random Number Generator (RNG)

