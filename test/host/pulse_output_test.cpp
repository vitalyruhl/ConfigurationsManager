// [MOCKED!] Only Arduino hardware/time calls are replaced; the helper is real.
#include "helpers/HelperModule.h"
#include <cassert>
#include <cstdio>

namespace {
struct Event { char operation; uint8_t pin; uint8_t value; };
Event events[256];
unsigned eventCount = 0;
unsigned long now = 0;
bool runtimeReady = false;
bool calledBeforeRuntime = false;
void record(char operation, uint8_t pin, uint8_t value) {
  if (!runtimeReady) calledBeforeRuntime = true;
  assert(eventCount < 256);
  events[eventCount++] = {operation, pin, value};
}
using cm::helpers::PulseOutput;
PulseOutput high(4);
PulseOutput low(5, PulseOutput::ActiveLevel::ActiveLow);
PulseOutput idle(7, PulseOutput::ActiveLevel::ActiveLow);
PulseOutput blocking(6, PulseOutput::ActiveLevel::ActiveLow);
#ifdef EXPECTED_BUILTIN_PIN
PulseOutput builtin;
#endif
void expect(unsigned index, char operation, uint8_t pin, uint8_t value) {
  assert(index < eventCount);
  assert(events[index].operation == operation);
  assert(events[index].pin == pin);
  assert(events[index].value == value);
}
}
void pinMode(uint8_t pin, uint8_t mode) { record('m', pin, mode); }
void digitalWrite(uint8_t pin, uint8_t value) { record('w', pin, value); }
unsigned long millis() {
  if (!runtimeReady) calledBeforeRuntime = true;
  return now;
}
void delay(unsigned long duration) { now += duration; }

int main() {
  // Catch hardware access from static constructors, including virtual RGB pins.
  assert(!calledBeforeRuntime);
  assert(eventCount == 0);
  high.setDefault(2, 20);
  assert(eventCount == 0);
  runtimeReady = true;

  // Initialize inactive before enabling OUTPUT, for either polarity.
  high.force(true);
  expect(0, 'w', 4, LOW);
  expect(1, 'm', 4, OUTPUT);
  expect(2, 'w', 4, HIGH);
  high.stop();
  expect(3, 'w', 4, LOW);
  low.force(true);
  expect(4, 'w', 5, HIGH);
  expect(5, 'm', 5, OUTPUT);
  expect(6, 'w', 5, LOW);
  low.stop();
  expect(7, 'w', 5, HIGH);

  // An otherwise idle loop must initialize OFF once, without toggling the LED.
  eventCount = 0;
  idle.loop();
  expect(0, 'w', 7, HIGH);
  expect(1, 'm', 7, OUTPUT);
  idle.loop();
  assert(eventCount == 2);

  // First use may be a blocking pulse; it must still preload the inactive latch.
  eventCount = 0;
  blocking.setPulseWait(1, 4);
  expect(0, 'w', 6, HIGH);
  expect(1, 'm', 6, OUTPUT);
  expect(2, 'w', 6, LOW);
  expect(3, 'w', 6, HIGH);

  // Existing one-shot timing and active-low completion remain intact.
  eventCount = 0;
  low.setPulse(1, 20);
  expect(0, 'w', 5, HIGH);
  low.loop();
  expect(1, 'w', 5, LOW);
  now += 10;
  low.loop();
  expect(2, 'w', 5, HIGH);
  now += 10;
  low.loop();
  low.loop();
  assert(eventCount == 3);

  // Blocking pulses preserve count, on/off polarity and final inactive state.
  eventCount = 0;
  const auto before = now;
  high.setPulseWait(2, 8);
  assert(now - before == 16);
  assert(eventCount == 4);
  expect(0, 'w', 4, HIGH);
  expect(3, 'w', 4, LOW);

#ifdef EXPECTED_BUILTIN_PIN
  eventCount = 0;
  builtin.force(true);
  expect(0, 'w', EXPECTED_BUILTIN_PIN, EXPECTED_BUILTIN_OFF);
  expect(1, 'm', EXPECTED_BUILTIN_PIN, OUTPUT);
  expect(2, 'w', EXPECTED_BUILTIN_PIN, !EXPECTED_BUILTIN_OFF);
  builtin.stop();
  expect(3, 'w', EXPECTED_BUILTIN_PIN, EXPECTED_BUILTIN_OFF);
#endif
  std::puts("[I] PulseOutput host checks passed");
}
