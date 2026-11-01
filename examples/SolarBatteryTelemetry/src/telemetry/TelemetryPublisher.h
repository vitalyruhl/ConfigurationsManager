#pragma once
#include "TelemetryChannels.h"
#include <limits>
namespace telemetry {
// The only network operations the portable coordinator can request. No receive or device-write API.
struct Transport {
 virtual ~Transport()=default;
 virtual bool connected() const=0;
 virtual bool publish(const char*,const char*,bool)=0;
 virtual void disconnect()=0;
 virtual void configure(const char*)=0;
};
class Publisher {
 char root_[rootLimit+1]="SolarBatteryTelemetry";
 char desired_[rootLimit+1]="SolarBatteryTelemetry";
 char retiring_[rootLimit+1]{};
 Value latest_[channelCount]{},snapshot_[channelCount]{},lastSent_[channelCount]{};
 static_assert(channelCount<=32,"The fixed field cache uses one bit per channel");
 uint32_t sentFields_=0,forceFields_=0,refreshAt_=0;bool refreshClockStarted_=false;
 uint8_t sentMetadata_=0;
 bool controlEnabled_=false,controlReady_=false;
 bool sourceAvailable_[2]{},snapshotAvailable_[2]{};
 std::size_t cursor_=0,retireCursor_=0;
 uint32_t completedAt_=0,retryAt_=0;
 bool changesPending_=false,pending_=true,delivered_=false,retry_=false,paused_=false,rejected_=false,advertised_=false;
 uint32_t intervalMs_=5000,fullRefreshMs_=60000;
 void resetCycle(){pending_=true;cursor_=0;delivered_=false;std::memcpy(snapshot_,latest_,sizeof(latest_));std::memcpy(snapshotAvailable_,sourceAvailable_,sizeof(sourceAvailable_));}
 void clearSent(){sentFields_=0;sentMetadata_=0;}
 bool publishOne(Transport& wire,const char* root,const char* suffix,const char* value,bool retain) {
  char topic[topicLimit+1];return buildTopic(root,suffix,topic,sizeof(topic))&&wire.publish(topic,value,retain);
 }
 bool retire(Transport& wire) {
  constexpr const char* suffixes[]={"bms/availability","victron/availability","status"};
  while(retireCursor_<3) {
   if(!publishOne(wire,retiring_,suffixes[retireCursor_],retireCursor_==2?"offline":"unavailable",true))return false;
   ++retireCursor_;
  }
  if(std::strcmp(retiring_,root_)==0)advertised_=false;
  retiring_[0]=0;retireCursor_=0;return true;
 }
public:
 bool requestRoot(const char* root,std::size_t n) {
  if(!validRoot(root,n)){rejected_=true;return false;}
  std::memcpy(desired_,root,n);desired_[n]=0;rejected_=false;return true;
 }
 const char* root() const{return root_;}
 bool rootRejected() const{return rejected_;}
 bool delivered() const{return delivered_;}
 void setPaused(bool paused){paused_=paused;}
 void setValue(std::size_t i,Value value){if(i>=channelCount)return;
  if(latest_[i].valid!=value.valid||(value.valid&&latest_[i].number!=value.number))changesPending_=true;
  latest_[i]=value;
 }
 void setSourceAvailable(Source source,bool available){
  const auto index=source==Source::Bms?0:1;
  if(sourceAvailable_[index]==available)return;
  sourceAvailable_[index]=available;
  sentMetadata_&=static_cast<uint8_t>(~((1u<<index)|(1u<<4)));
  for(std::size_t i=0;i<channelCount;++i)if(channels[i].source==source){
   sentFields_&=~(uint32_t{1}<<i);
   if(!available)latest_[i]=Value{};
  }
  resetCycle();
 }
 void setControl(bool enabled,bool ready){if(enabled==controlEnabled_&&ready==controlReady_)return;controlEnabled_=enabled;controlReady_=ready;sentMetadata_&=static_cast<uint8_t>(~((1u<<2)|(1u<<4)));resetCycle();}
 void onConnected(){advertised_=true;clearSent();forceFields_=0;refreshClockStarted_=false;retry_=false;resetCycle();}
 void setInterval(uint32_t ms){intervalMs_=ms<5000?5000:ms;}
 void setRefreshInterval(uint32_t ms){fullRefreshMs_=ms<5000?5000:ms>3600000?3600000:ms;}
 // Configuration disable/reconnect uses the same bounded retirement path.
 bool retireCurrent(Transport& wire) {
  // Drain a prior advertised root first, then retire this connection's current advertisement too.
  if(retiring_[0]&&(!wire.connected()||!retire(wire)))return false;
  if(!advertised_)return true;
  std::memcpy(retiring_,root_,sizeof(root_));retireCursor_=0;
  return wire.connected()&&retire(wire);
 }
 bool retirementPending() const{return retiring_[0]!=0;}
 void abandonOldEndpoint(){retiring_[0]=0;retireCursor_=0;advertised_=false;}

 void tick(uint32_t now,Transport& wire,unsigned stepBudget=4) {
  if(paused_)return;
  if(retry_&&static_cast<uint32_t>(now-retryAt_)<250)return;
  retry_=false;
  if(std::strcmp(root_,desired_)!=0) {
   // One advertised-root obligation plus a separate latest desired root; never queue history.
   if(retiring_[0]&&(!wire.connected()||!retire(wire))){retry_=true;retryAt_=now;return;}
   if(advertised_) {
    std::memcpy(retiring_,root_,sizeof(root_));retireCursor_=0;
    if(wire.connected()&&!retire(wire)){retry_=true;retryAt_=now;return;}
   }
   wire.disconnect();std::memcpy(root_,desired_,sizeof(root_));advertised_=false;
   wire.configure(root_);clearSent();resetCycle();return;
  }
  if(!wire.connected())return;
  if(retiring_[0]&&!retire(wire)){retry_=true;retryAt_=now;return;}
  // A receiver restart must recover unchanged data without restarting this collector.
  // Refresh current metadata/valid fields on the configured cadence, independently of deltas.
  if(!refreshClockStarted_){refreshClockStarted_=true;refreshAt_=now;}
  else if(static_cast<uint32_t>(now-refreshAt_)>=fullRefreshMs_){refreshAt_=now;forceFields_=~uint32_t{0};sentMetadata_=0;resetCycle();}
  // Finish the bounded in-flight snapshot so changing early fields cannot starve later MOS fields.
  if(changesPending_&&!pending_){changesPending_=false;resetCycle();}
  if(!pending_&&static_cast<uint32_t>(now-completedAt_)<intervalMs_)return;
  if(!pending_)resetCycle();
  // Four bounded cursor steps (at most four admissions). Skips never allocate or send.
  // Keep successful values only; failed admissions retain this snapshot and retry cursor.
  for(unsigned budget=0;budget<stepBudget&&cursor_<channelCount+5;++budget) {
   bool ok=false;
   if(cursor_<5) {
    const auto bit=static_cast<uint8_t>(1u<<cursor_);
    if(sentMetadata_&bit){++cursor_;continue;}
    if(cursor_==0)ok=publishOne(wire,root_,"bms/availability",snapshotAvailable_[0]?"available":"unavailable",true);
    else if(cursor_==1)ok=publishOne(wire,root_,"victron/availability",snapshotAvailable_[1]?"available":"unavailable",true);
    else if(cursor_==2)ok=publishOne(wire,root_,"bms/control/availability",controlReady_?"available":controlEnabled_?"unavailable":"disabled",true);
    else if(cursor_==3)ok=publishOne(wire,root_,"status","online",true);
    else {char payload[payloadLimit+1];ok=capabilities(root_,payload,sizeof(payload),snapshotAvailable_[1],snapshotAvailable_[0],controlEnabled_)&&publishOne(wire,root_,"capabilities",payload,true);}
    if(ok){sentMetadata_|=bit;if(cursor_==3)advertised_=true;}
   } else {
    const auto i=cursor_-5;
    const auto bit=uint32_t{1}<<i;
    if(!snapshotAvailable_[channels[i].source==Source::Bms?0:1]){++cursor_;continue;}
    // Protection descriptions are bounded at 512 bytes; compare serialized values.
    char payload[512],previous[512];
    if(formatValue(channels[i],snapshot_[i],payload,sizeof(payload))) {
     if(!(sentFields_&bit)) {
      // No initial unknown/unsupported field burst; availability carries source loss.
      if(std::strcmp(payload,"null")==0){++cursor_;continue;}
     } else if(!((forceFields_&bit)&&std::strcmp(payload,"null"))&&formatValue(channels[i],lastSent_[i],previous,sizeof(previous))) {
      if(std::strcmp(payload,previous)==0){++cursor_;continue;}
      // Deadband affects admission only; retain full payload precision and validity transitions.
      if(channels[i].type==FieldType::Real&&std::strcmp(payload,"null")&&std::strcmp(previous,"null")) {
       const double threshold=std::strcmp(channels[i].unit,"W")==0?1.0:0.1;
       const double delta=std::fabs(snapshot_[i].number-lastSent_[i].number);
       // Accept exact decimal boundaries despite binary floating-point subtraction error.
       const double scale=std::fmax(1.0,std::fmax(std::fabs(snapshot_[i].number),std::fabs(lastSent_[i].number)));
       const double tolerance=8*std::numeric_limits<double>::epsilon()*scale;
       if(delta<threshold-tolerance){++cursor_;continue;}
      }
     }
     const bool retained=i==channelIndex("bms/charge_enabled")||i==channelIndex("bms/discharge_enabled")||i==channelIndex("victron/error_code")||i==channelIndex("victron/yield_today_wh");
     ok=publishOne(wire,root_,channels[i].suffix,payload,retained);
     if(ok){lastSent_[i]=snapshot_[i];sentFields_|=bit;forceFields_&=~bit;}
    }
   }
   if(!ok){retry_=true;retryAt_=now;return;}
   ++cursor_;
  }
  if(cursor_==channelCount+5){pending_=false;delivered_=true;completedAt_=now;}
 }
};
}
