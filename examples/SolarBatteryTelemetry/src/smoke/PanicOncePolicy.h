#pragma once
#include <cstdint>
namespace sbt::testprofile {
struct PanicOnceLatch {uint32_t magic,inverse;};
// Two RTC words survive a panic reset. No NVS or production setting is involved.
class PanicOncePolicy {
 // Generation 2 re-arms only the explicit 1.4.10 COM8 fixture after a 1.4.9 test.
 static constexpr uint32_t magic=0x53425032u;
 bool timing_=false;uint32_t healthyAt_=0;
public:
 bool ready(uint32_t now,bool healthy,volatile PanicOnceLatch& rtc){
  if(rtc.magic==magic&&rtc.inverse==~magic)return false;
  if(!healthy){timing_=false;return false;}
  if(!timing_){timing_=true;healthyAt_=now;return false;}
  if(uint32_t(now-healthyAt_)<60000)return false;
  rtc.magic=magic;rtc.inverse=~magic;
  return true;
 }
};
}
