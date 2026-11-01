#pragma once
#include "telemetry/TelemetryChannels.h"
#include <algorithm>
#include <array>
namespace bms {
struct Frame { uint8_t reg=0,size=0,status=0; std::array<uint8_t,255> payload{}; };
inline bool readCommand(uint8_t reg,uint8_t (&out)[7]) {
 if(reg!=3&&reg!=4)return false;
 const uint8_t request[]={0xdd,0xa5,reg,0,0xff,uint8_t(0-reg),0x77};
 std::memcpy(out,request,7);return true;
}
// One bounded frame. Call reset on query/connection changes or the two-second deadline.
class Parser {
 std::array<uint8_t,262> bytes_{};size_t size_=0;
public:
 void reset(){size_=0;}
 bool push(uint8_t byte,Frame& out){
  if(!size_&&byte!=0xdd)return false;
  bytes_[size_++]=byte;
  if(size_<4)return false;
  const size_t length=bytes_[3],total=length+7;
  if(size_<total)return false;
  size_=0;
  if(bytes_[total-1]!=0x77||(bytes_[1]!=3&&bytes_[1]!=4&&bytes_[1]!=0xe1))return false;
  uint16_t sum=0;for(size_t i=2;i<length+4;++i)sum+=bytes_[i];
  const uint16_t checksum=(uint16_t(bytes_[length+4])<<8)|bytes_[length+5];
  if(uint16_t(sum+checksum)!=0)return false;
  out.reg=bytes_[1];out.status=bytes_[2];out.size=length;std::memcpy(out.payload.data(),bytes_.data()+4,length);return true;
 }
};
struct Snapshot {std::array<telemetry::Value,telemetry::channelCount> values{};std::array<uint16_t,14> cellsMv{};bool cellsValid=false,available=false;uint32_t receivedAt=0,sequence=0;};
inline uint16_t u16(const uint8_t* p){return (uint16_t(p[0])<<8)|p[1];}
class Cycle {
 Snapshot pending_{},published_{};bool basic_=false,have_=false;uint32_t basicAt_=0,publishedAt_=0,sequence_=0;
public:
 // A transport interruption invalidates the partial pair, never its last complete sample.
 void interrupt(){pending_={};basic_=false;}
 void reset(){interrupt();published_={};have_=false;}
 bool accept(const Frame& f,uint32_t now){
  if(f.status){basic_=false;return false;}
  const auto* p=f.payload.data();
  if(f.reg==3){
   basic_=false;pending_={};
   if(f.size<23||p[21]!=14||p[22]>8||f.size<23+2*p[22]||p[19]>100||!u16(p)||!u16(p+6))return false;
   const double voltage=u16(p)*.01;
   const uint16_t rawCurrent=u16(p+2);const int32_t signedCurrent=rawCurrent<32768?rawCurrent:int32_t(rawCurrent)-65536;
   const double current=signedCurrent*.01;
   auto set=[this](size_t i,double value){pending_.values[i]={true,value};};
   set(0,current>0);set(1,current<0);set(2,p[19]);set(3,voltage);set(4,current);set(5,voltage*current);
   set(6,u16(p+4)*.01);set(7,u16(p+6)*.01);set(8,(p[20]&1)!=0);set(9,(p[20]&2)!=0);set(14,u16(p+16));
   set(telemetry::channelIndex("bms/protection"),u16(p+16));
   if(p[22]){double hottest=-273.1,coldest=1000;for(size_t i=0;i<p[22];++i){const double t=(int32_t(u16(p+23+2*i))-2731)*.1;
     if(t < -50 || t > 150)return false;
     if(i<2)set(telemetry::channelIndex(i==0?"bms/temperature_1_c":"bms/temperature_2_c"),t);
     hottest=std::max(hottest,t);coldest=std::min(coldest,t);}set(10,hottest);set(telemetry::channelIndex("bms/temperature_min_c"),coldest);}
   basic_=true;basicAt_=now;return true;
  }
  if(f.reg!=4)return false;
  const bool ready=basic_&&uint32_t(now-basicAt_)<2000;basic_=false;
  if(!ready||f.size!=28)return false;
  uint16_t low=65535,high=0;
  for(size_t i=0;i<14;++i){auto value=u16(p+2*i);if(!value||value>6000)return false;pending_.cellsMv[i]=value;low=std::min(low,value);high=std::max(high,value);}
  pending_.cellsValid=true;
  pending_.values[11]={true,low*.001};pending_.values[12]={true,high*.001};pending_.values[13]={true,(high-low)*.001};
  pending_.receivedAt=now;pending_.sequence=++sequence_;published_=pending_;publishedAt_=now;have_=true;return true;
 }
 Snapshot snapshot(uint32_t now,uint32_t timeoutMs=30000) const {auto result=published_;result.available=have_&&uint32_t(now-publishedAt_)<timeoutMs;
  if(!result.available){result.values={};result.cellsValid=false;}return result;}
};
}
