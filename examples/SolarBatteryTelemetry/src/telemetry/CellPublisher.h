#pragma once
#include "TelemetryPublisher.h"
#include "bms/JbdProtocol.h"
namespace telemetry {
// Independent of the scalar bit cache. Only one checked 03/04 pair is retained.
class CellPublisher {
 std::array<uint16_t,14> latest_{},sent_{};
 uint32_t sequence_=0,receivedMs_=0,refreshAt_=0,sentAt_=0,retryAt_=0;
 int64_t acquiredAt_=0;
 bool available_=false,have_=false,delivered_=false,sentAvailable_=false,retry_=false,refreshStarted_=false;
 uint32_t intervalMs_=5000,refreshMs_=60000;
 char root_[rootLimit+1]{};
public:
 void observe(const bms::Snapshot& sample,uint32_t now,int64_t unixNow){
  const bool valid=sample.available&&sample.cellsValid;
  available_=valid;
  if(!valid)return;
  if(!have_||sequence_!=sample.sequence||receivedMs_!=sample.receivedAt){
   const auto age=uint32_t(now-sample.receivedAt)/1000u;
   acquiredAt_=unixNow>=1577836800&&unixNow-age>=1577836800?unixNow-age:0;
  }
  have_=true;sequence_=sample.sequence;receivedMs_=sample.receivedAt;latest_=sample.cellsMv;
 }
 void setInterval(uint32_t ms){intervalMs_=ms<5000?5000:ms;}
 void setRefreshInterval(uint32_t ms){refreshMs_=ms<5000?5000:ms>3600000?3600000:ms;}
 void onConnected(){delivered_=false;retry_=false;refreshStarted_=false;}
 void tick(uint32_t now,const char* root,Transport& wire){
  if(!wire.connected()||!root||!validRoot(root,std::strlen(root)))return;
  if(std::strcmp(root_,root)){std::memcpy(root_,root,std::strlen(root)+1);onConnected();}
  if(retry_&&uint32_t(now-retryAt_)<250)return;
  if(!refreshStarted_){refreshAt_=now;refreshStarted_=true;}
  const bool refresh=uint32_t(now-refreshAt_)>=refreshMs_;
  const bool transition=!delivered_||available_!=sentAvailable_;
  if(!transition&&!refresh&&(!available_||latest_==sent_||uint32_t(now-sentAt_)<intervalMs_))return;
  char topic[topicLimit+1],payload[320],age[20]="null",utc[24]="null";
  if(have_)std::snprintf(age,sizeof(age),"%lu",static_cast<unsigned long>(uint32_t(now-receivedMs_)));
  if(acquiredAt_)std::snprintf(utc,sizeof(utc),"%lld",static_cast<long long>(acquiredAt_));
  int n=std::snprintf(payload,sizeof(payload),"{\"available\":%s,\"source\":\"jbd_03_04\",\"sample_sequence\":%lu,\"sample_age_ms\":%s,\"received_at\":%s,\"cells_mv\":",available_?"true":"false",static_cast<unsigned long>(sequence_),age,utc);
  if(n<0||static_cast<size_t>(n)>=sizeof(payload))return;
  size_t used=static_cast<size_t>(n);
  if(available_){
   for(size_t i=0;i<latest_.size();++i){
    n=std::snprintf(payload+used,sizeof(payload)-used,"%s%u",i?",":"[",static_cast<unsigned>(latest_[i]));
    if(n<0||static_cast<size_t>(n)>=sizeof(payload)-used)return;used+=static_cast<size_t>(n);
   }
  }
  n=std::snprintf(payload+used,sizeof(payload)-used,available_?"]}":"null}");
  if(n<0||static_cast<size_t>(n)>=sizeof(payload)-used||!buildTopic(root_,"bms/cells",topic,sizeof(topic)))return;
  if(!wire.publish(topic,payload,false)){retry_=true;retryAt_=now;return;}
  sent_=latest_;sentAvailable_=available_;delivered_=true;sentAt_=now;retry_=false;
  if(refresh)refreshAt_=now;
 }
};
}
