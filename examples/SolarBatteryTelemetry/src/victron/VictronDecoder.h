#pragma once
#include "../telemetry/TelemetryChannels.h"
namespace victron {
inline constexpr uint32_t staleAfterMs=30000;
inline int hexDigit(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
inline bool validMac(const char* text,std::size_t size){
    if(!text||(size!=12&&size!=17))return false;
    for(std::size_t i=0;i<size;++i)if(size==17&&i%3==2?text[i]!=':':hexDigit(text[i])<0)return false;
    return true;
}
inline bool parseKey(const char* text,std::size_t size,uint8_t* out){
    if(!text||!out||size!=32)return false;
    uint8_t parsed[16]{};
    for(std::size_t i=0;i<16;++i){const int a=hexDigit(text[i*2]),b=hexDigit(text[i*2+1]);if(a<0||b<0)return false;parsed[i]=static_cast<uint8_t>((a<<4)|b);}
    std::memcpy(out,parsed,sizeof(parsed));return true;
}
struct Reading {telemetry::Value values[telemetry::channelCount]{};uint16_t counter=0,productId=0;};
struct Snapshot {telemetry::Value values[telemetry::channelCount]{};bool available=false;const char* reason="configuration_missing";uint32_t receivedMs=0;uint16_t productId=0;int8_t rssi=0;};
enum class DecodeResult {Ok,Ignored,Truncated,Unsupported,KeyMismatch,CryptoError};
inline uint16_t little16(const uint8_t* p){return static_cast<uint16_t>(p[0]|uint16_t(p[1])<<8);}
inline int signed16(uint16_t value){return value&0x8000?int(value)-65536:int(value);}
inline unsigned chargeState(uint8_t value){
    switch(value){case 0:return 0;case 2:return 1;case 3:return 2;case 4:return 3;case 5:return 4;case 6:return 5;case 7:case 247:return 6;default:return 7;}
}
// Manufacturer bytes include company ID; encrypted Solar Charger payload starts at byte 10.
// Only the documented 12-byte prefix is needed, so future trailing extensions are ignored.
// AES-CTR provides no authentication: the key-check byte is only an early mismatch check.
template<class Decrypt> DecodeResult decode(const uint8_t* data,std::size_t size,const uint8_t* key,Decrypt decrypt,Reading& result){
    if(!data||!key||size<10)return DecodeResult::Truncated;
    if(little16(data)!=0x02e1||data[2]!=0x10)return DecodeResult::Ignored;
    if(data[6]!=0x01)return DecodeResult::Unsupported;
    if(size<22)return DecodeResult::Truncated;
    if(data[9]!=key[0])return DecodeResult::KeyMismatch;
    uint8_t nonce[16]={data[7],data[8]},plain[12]{};
    if(!decrypt(key,nonce,data+10,sizeof(plain),plain))return DecodeResult::CryptoError;
    Reading next;next.counter=little16(data+7);next.productId=little16(data+4);
    auto field=[&](unsigned index,uint16_t raw,uint16_t na,double scale,bool isSigned=false){
        if(raw!=na)next.values[index]={true,(isSigned?signed16(raw):int(raw))*scale};
    };
    field(telemetry::channelIndex("victron/solar_power_w"),little16(plain+8),0xffff,1);field(telemetry::channelIndex("victron/battery_voltage_v"),little16(plain+2),0x7fff,0.01,true);
    field(telemetry::channelIndex("victron/charge_current_a"),little16(plain+4),0x7fff,0.1,true);field(telemetry::channelIndex("victron/load_current_a"),little16(plain+10)&0x1ff,0x1ff,0.1);
    if(plain[0]!=0xff)next.values[telemetry::channelIndex("victron/charge_state")]={true,double(chargeState(plain[0]))};
    field(telemetry::channelIndex("victron/error_code"),plain[1],0xff,1);field(telemetry::channelIndex("victron/yield_today_wh"),little16(plain+6),0xffff,10);
    result=next;return DecodeResult::Ok;
}
class Receiver {
    Reading latest_{};uint32_t receivedMs_=0;int8_t rssi_=0;bool haveSample_=false,keyMismatch_=false;
    const char* reason_="configuration_missing";
public:
    void reset(const char* reason){latest_={};receivedMs_=0;rssi_=0;haveSample_=false;keyMismatch_=false;reason_=reason;}
    template<class Decrypt> bool receive(const uint8_t* data,std::size_t size,const uint8_t* key,uint32_t receivedMs,int8_t rssi,Decrypt decrypt){
        if(keyMismatch_)return false;
        Reading next;const auto status=decode(data,size,key,decrypt,next);
        if(status==DecodeResult::KeyMismatch){haveSample_=false;keyMismatch_=true;reason_="key_mismatch";return false;}
        if(status!=DecodeResult::Ok){if(!haveSample_)reason_=status==DecodeResult::Unsupported?"unsupported_record":"waiting_for_advertisement";return false;}
        if(haveSample_&&next.counter==latest_.counter)return false;
        latest_=next;receivedMs_=receivedMs;rssi_=rssi;haveSample_=true;reason_="fresh";return true;
    }
    Snapshot snapshot(uint32_t now) const{
        Snapshot out;out.receivedMs=receivedMs_;out.productId=latest_.productId;out.rssi=rssi_;out.reason=reason_;
        if(haveSample_){out.available=uint32_t(now-receivedMs_)<staleAfterMs;out.reason=out.available?"fresh":"stale";
            if(out.available)std::memcpy(out.values,latest_.values,sizeof(out.values));}
        return out;
    }
};
}
