#pragma once
#include "OperatingModes.h"
namespace control {
constexpr size_t kModeProfileBytes=16;
inline uint32_t modeProfileChecksum(const uint8_t *data) {
    uint32_t hash=2166136261UL;
    for(size_t i=0;i<12;++i) hash=(hash^data[i])*16777619UL;
    return hash;
}
inline void encodeModeProfile(const ModeSettings &settings,uint8_t *data) {
    static_assert(sizeof(float)==4,"Mode profile requires 32-bit float");
    std::memset(data,0,kModeProfileBytes);
    std::memcpy(data,"SILM",4); data[4]=1;
    data[5]=static_cast<uint8_t>(settings.mode); data[6]=uint8_t(settings.inverterSharePercent);
    std::memcpy(data+8,&settings.chargeThreshold,4);
    const uint32_t checksum=modeProfileChecksum(data);
    for(unsigned i=0;i<4;++i) data[12+i]=uint8_t(checksum>>(8*i));
}
inline bool decodeModeProfile(const uint8_t *data,size_t length,ModeSettings &out) {
    if(!data || length!=kModeProfileBytes || std::memcmp(data,"SILM",4) || data[4]!=1 || data[7]!=0) return false;
    const uint32_t checksum=modeProfileChecksum(data);
    for(unsigned i=0;i<4;++i) if(data[12+i]!=uint8_t(checksum>>(8*i))) return false;
    ModeSettings candidate; candidate.mode=static_cast<OperatingMode>(data[5]);
    candidate.inverterSharePercent=data[6]; std::memcpy(&candidate.chargeThreshold,data+8,4);
    if(!validModeSettings(candidate)) return false;
    out=candidate; return true;
}
template<class Write,class Read,class Activate> bool persistModeProfile(const ModeSettings &settings,Write write,Read read,Activate activate) {
    if(!validModeSettings(settings)) return false;
    uint8_t record[kModeProfileBytes],actual[kModeProfileBytes]={};
    encodeModeProfile(settings,record);
    if(write(record,sizeof(record))!=sizeof(record) || read(actual,sizeof(actual))!=sizeof(actual)) return false;
    ModeSettings verified;
    if(std::memcmp(record,actual,sizeof(record)) || !decodeModeProfile(actual,sizeof(actual),verified)) return false;
    // NVS atomically replaces one key only after the staged bytes are verified.
    // A full successful activation write is the commit acknowledgement; a
    // post-commit read error must not turn a durable activation into save_failed.
    return activate(record,sizeof(record))==sizeof(record);
}
}
