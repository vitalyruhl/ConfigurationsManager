#pragma once
// [MOCKED!] Arduino hardware boundary for production PulseOutput host tests.
#include <cstdint>
constexpr uint8_t LOW = 0;
constexpr uint8_t HIGH = 1;
constexpr uint8_t OUTPUT = 3;
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
unsigned long millis();
void delay(unsigned long duration);
