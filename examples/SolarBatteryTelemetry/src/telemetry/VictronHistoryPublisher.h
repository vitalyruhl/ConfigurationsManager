#pragma once
#include "TelemetryPublisher.h"
namespace telemetry {
// Display history is independent of live measurement validity and never used for control.
class VictronHistoryPublisher {
 struct Slot {Value value{};uint32_t receivedMs=0;int64_t receivedAt=0;uint32_t sentAt=0;bool valid=false,dirty=false,sent=false;} slots_[2];
 uint32_t retryAt_=0;bool retry_=false;unsigned cursor_=0;
 char root_[rootLimit+1]{};
public:
 void observe(const Value* values,bool available,uint32_t receivedMs,uint32_t now,int64_t unixNow) {
  if(!available||!values)return;
  constexpr std::size_t fields[]={channelIndex("victron/error_code"),channelIndex("victron/yield_today_wh")};
  for(unsigned i=0;i<2;++i){
   auto& slot=slots_[i];if(slot.valid&&slot.receivedMs==receivedMs)continue;
   char number[32];const auto field=fields[i];
   if(!formatValue(channels[field],values[field],number,sizeof(number))||!std::strcmp(number,"null"))continue;
   const auto ageSeconds=uint32_t(now-receivedMs)/1000u;
   const int64_t receivedAt=unixNow>=1577836800&&unixNow-ageSeconds>=1577836800?unixNow-ageSeconds:0;
   slot.value=values[field];slot.receivedMs=receivedMs;slot.receivedAt=receivedAt;slot.valid=slot.dirty=true;
  }
 }
 void onConnected(){for(auto& slot:slots_){slot.dirty=slot.valid;slot.sent=false;}retry_=false;}
 void tick(uint32_t now,const char* root,Transport& wire) {
  if(!wire.connected()||!root||!validRoot(root,std::strlen(root)))return;
  if(std::strcmp(root_,root)){std::memcpy(root_,root,std::strlen(root)+1);onConnected();}
  if(retry_&&uint32_t(now-retryAt_)<250)return;
  constexpr const char* suffixes[]={"victron/last_valid/error_code","victron/last_valid/yield_today_wh"};
  constexpr std::size_t fields[]={channelIndex("victron/error_code"),channelIndex("victron/yield_today_wh")};
  for(unsigned step=0;step<2;++step){
   const auto i=(cursor_+step)%2;auto& slot=slots_[i];
   if(!slot.dirty||(slot.sent&&uint32_t(now-slot.sentAt)<5000))continue;
   char topic[topicLimit+1],number[32],time[24]="null",payload[128];
   if(!buildTopic(root_,suffixes[i],topic,sizeof(topic))||!formatValue(channels[fields[i]],slot.value,number,sizeof(number)))return;
   if(slot.receivedAt)std::snprintf(time,sizeof(time),"%lld",static_cast<long long>(slot.receivedAt));
   std::snprintf(payload,sizeof(payload),"{\"value\":%s,\"received_at\":%s}",number,time);
   if(!wire.publish(topic,payload,true)){retry_=true;retryAt_=now;return;}
   slot.dirty=false;slot.sent=true;slot.sentAt=now;retry_=false;cursor_=(i+1)%2;
   // One admission per loop, at most once per field per five seconds, with fair retries.
   return;
  }
  retry_=false;
 }
};
}
