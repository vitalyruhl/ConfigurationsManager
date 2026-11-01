#pragma once
#ifndef SBT_TEST_PANIC_ONCE
#define SBT_TEST_PANIC_ONCE 0
#endif
#if SBT_TEST_PANIC_ONCE != 0 && SBT_TEST_PANIC_ONCE != 1
#error "SBT_TEST_PANIC_ONCE must be 0 or 1"
#endif
#if SBT_TEST_PANIC_ONCE
#if !defined(SBT_ISOLATED_TEST_PROFILE) || SBT_ISOLATED_TEST_PROFILE != 1
#error "Deliberate panic requires SBT_ISOLATED_TEST_PROFILE=1"
#endif
#include "PanicOncePolicy.h"
#include <Arduino.h>
#include <esp_attr.h>
#include <cstdlib>
namespace sbt::testprofile {
static_assert(isolated,"A deliberate panic cannot run in the production profile");
RTC_NOINIT_ATTR static volatile PanicOnceLatch panicOnceRtc;
[[noreturn]] __attribute__((noinline)) static void deliberateTestPanicOnce(){
 Serial.println("SBT_TEST_PANIC_ONCE_MARKER");
 std::abort();
}
inline void tickPanicOnceProbe(uint32_t now,bool healthy){
 static PanicOncePolicy policy;
 if(policy.ready(now,healthy,panicOnceRtc)){
  // Publish both RTC writes before the deliberate reset can happen.
  __sync_synchronize();
  deliberateTestPanicOnce();
 }
}
}
#endif
