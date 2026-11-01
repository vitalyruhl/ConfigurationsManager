#pragma once
#include "TelemetryPublisher.h"
namespace telemetry {
inline bool formatCommandResult(const char* id,const char* phase,const char* reason,const Value* values,bool available,char* out,size_t capacity,const char* session=""){
 char charge[8]="null",discharge[8]="null",current[32]="null";
 if(available&&values){formatValue(channels[8],values[8],charge,sizeof(charge));formatValue(channels[9],values[9],discharge,sizeof(discharge));formatValue(channels[4],values[4],current,sizeof(current));}
 const int n=std::snprintf(out,capacity,"{\"request_id\":\"%s\",\"phase\":\"%s\",\"reason\":\"%s\",\"charge_enabled\":%s,\"discharge_enabled\":%s,\"observed_current_a\":%s,\"session\":\"%s\"}",id,phase,reason,charge,discharge,current,session);
 return n>=0&&static_cast<size_t>(n)<capacity;
}

inline bool formatControlStatus(bool enabled,bool ready,bool pending,const char* session,const char* reason,bool known,uint8_t mask,const char* readback,char* out,size_t capacity,const char* peer=""){
 const int n=std::snprintf(out,capacity,"{\"enabled\":%s,\"ready\":%s,\"pending\":%s,\"session\":\"%s\",\"reason\":\"%s\",\"permissions_known\":%s,\"requested_charge_enabled\":%s,\"requested_discharge_enabled\":%s,\"mos_readback\":\"%s\",\"peer\":\"%s\"}",
  enabled?"true":"false",ready?"true":"false",pending?"true":"false",session,reason,known?"true":"false",known?(mask&1?"true":"false"):"null",known?(mask&2?"true":"false"):"null",readback,peer);
 return n>=0&&static_cast<size_t>(n)<capacity;
}
inline bool formatAcknowledgedPermissions(const char* peer,uint8_t mask,char* out,size_t capacity){
 const int n=std::snprintf(out,capacity,"{\"peer\":\"%s\",\"charge_enabled\":%s,\"discharge_enabled\":%s}",peer,mask&1?"true":"false",mask&2?"true":"false");
 return n>=0&&static_cast<size_t>(n)<capacity;
}
class StatusPublisher {
 struct Slot {bool valid=false,dirty=false;};Slot slots_[5]{};
 // Match the existing health/control producers; each slot keeps independent retry ownership.
 char payloads_[256+256+384+384+128]{};
 static constexpr size_t capacity(unsigned index){return index<2?256:index==4?128:384;}
 char* payload(unsigned index){return payloads_+(index<2?index*256:index==4?1280:512+(index-2)*384);}
 uint32_t refreshAt_=0,refreshMs_=60000;bool refreshStarted_=false;
 uint32_t retryAt_=0;bool retry_=false;unsigned cursor_=0;
public:
 bool set(unsigned index,const char* payload,bool force=false){
  if(index>=5||!payload||std::strlen(payload)>=capacity(index))return false;
  auto& slot=slots_[index];char* stored=this->payload(index);if(force||!slot.valid||std::strcmp(stored,payload)){std::memcpy(stored,payload,std::strlen(payload)+1);slot.valid=slot.dirty=true;}return true;
 }
 void clearPermissions(){slots_[4]={};}
 void setRefreshInterval(uint32_t ms){refreshMs_=ms<5000?5000:ms>3600000?3600000:ms;}
 void clearResult(){slots_[3]={};std::memset(payload(3),0,capacity(3));}
 void onConnected(){clearResult();for(auto& slot:slots_)slot.dirty=slot.valid;retry_=false;refreshStarted_=false;}
 // mode 1 prioritizes control/results, mode 2 leaves those for the urgent path.
 void tick(uint32_t now,const char* root,Transport& wire,unsigned mode=0){
  if(!wire.connected()||(retry_&&uint32_t(now-retryAt_)<250))return;retry_=false;
  if(!refreshStarted_){refreshStarted_=true;refreshAt_=now;}
  else if(uint32_t(now-refreshAt_)>=refreshMs_){refreshAt_=now;for(auto& slot:slots_)slot.dirty=slot.valid;}
  constexpr const char* suffixes[]={"bms/health","victron/health","bms/control","bms/control/result","bms/permissions/acknowledged"};
  for(unsigned i=0;i<5;++i){const auto index=(cursor_+i)%5;if((mode==1&&index<2)||(mode==2&&index>=2))continue;auto& slot=slots_[index];if(!slot.dirty)continue;
   char topic[topicLimit+1];if(!buildTopic(root,suffixes[index],topic,sizeof(topic)))return;
   if(wire.publish(topic,payload(index),index!=3)){slot.dirty=false;cursor_=(index+1)%5;}
   else {retry_=true;retryAt_=now;}return;
  }
 }
};
}
