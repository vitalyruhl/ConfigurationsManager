#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
namespace telemetry {
inline constexpr std::size_t rootLimit=63,topicLimit=128,payloadLimit=1024;
enum class Source { Bms, Victron };
enum class FieldType { Boolean, Real, Unsigned, State, Protection };
enum class Support { PendingAdapter, Unsupported, DerivedLater, InstantReadout, JbdReadout };
struct Channel { Source source; FieldType type; const char* suffix; const char* label; const char* unit; Support support=Support::JbdReadout; };
inline constexpr Channel channels[]={
 {Source::Bms,FieldType::Boolean,"bms/charging","Charging",""},
 {Source::Bms,FieldType::Boolean,"bms/discharging","Discharging",""},
 {Source::Bms,FieldType::Real,"bms/state_of_charge_pct","State of charge","%"},
 {Source::Bms,FieldType::Real,"bms/voltage_v","Battery voltage","V"},
 {Source::Bms,FieldType::Real,"bms/current_a","Battery current","A"},
 {Source::Bms,FieldType::Real,"bms/power_w","Battery power","W"},
 {Source::Bms,FieldType::Real,"bms/remaining_capacity_ah","Remaining capacity","Ah"},
 {Source::Bms,FieldType::Real,"bms/full_capacity_ah","Full capacity","Ah"},
 {Source::Bms,FieldType::Boolean,"bms/charge_enabled","Charge MOS enabled",""},
 {Source::Bms,FieldType::Boolean,"bms/discharge_enabled","Discharge MOS enabled",""},
 {Source::Bms,FieldType::Real,"bms/temperature_max_c","Maximum temperature","C"},
 {Source::Bms,FieldType::Real,"bms/cell_voltage_min_v","Minimum cell voltage","V"},
 {Source::Bms,FieldType::Real,"bms/cell_voltage_max_v","Maximum cell voltage","V"},
 {Source::Bms,FieldType::Real,"bms/cell_voltage_delta_v","Cell voltage spread","V"},
 {Source::Bms,FieldType::Unsigned,"bms/alarm_bits","Alarm bits",""},
 {Source::Victron,FieldType::Real,"victron/solar_power_w","Solar power","W",Support::InstantReadout},
 {Source::Victron,FieldType::Real,"victron/battery_voltage_v","Charger battery voltage","V",Support::InstantReadout},
 {Source::Victron,FieldType::Real,"victron/charge_current_a","Charger output current","A",Support::InstantReadout},
 {Source::Victron,FieldType::Real,"victron/load_current_a","Load current","A",Support::InstantReadout},
 {Source::Victron,FieldType::State,"victron/charge_state","Charge state","",Support::InstantReadout},
 {Source::Victron,FieldType::Unsigned,"victron/error_code","Error code","",Support::InstantReadout},
 {Source::Victron,FieldType::Real,"victron/yield_today_wh","Yield today","Wh",Support::InstantReadout},
 {Source::Bms,FieldType::Real,"bms/temperature_min_c","Minimum NTC temperature","C"},
 {Source::Bms,FieldType::Protection,"bms/protection","Active protections",""},
 {Source::Bms,FieldType::Real,"bms/temperature_1_c","NTC 1 (Top)","C"},
 {Source::Bms,FieldType::Real,"bms/temperature_2_c","NTC 2 (Bottom)","C"}
};
inline constexpr std::size_t channelCount=sizeof(channels)/sizeof(channels[0]);
// Resolve stable public suffixes at compile time; decoder slots follow the schema.
inline constexpr std::size_t channelIndex(const char* suffix) {
 for(std::size_t i=0;i<channelCount;++i){std::size_t n=0;while(channels[i].suffix[n]&&suffix[n]&&channels[i].suffix[n]==suffix[n])++n;
  if(channels[i].suffix[n]==suffix[n])return i;}
 return channelCount;
}
inline constexpr const char* chargeStates[]={"off","fault","bulk","absorption","float","storage","equalization","other","unavailable"};
inline constexpr const char* commandSuffixes[]={"bms/charge_enabled/set","bms/discharge_enabled/set","bms/permissions/set","bms/control_enabled/set","bms/emergency_off/set"};
inline const char* sourceName(Source s){return s==Source::Bms?"bms":"victron";}
inline const char* supportName(Support s){return s==Support::JbdReadout?"jbd_readout":s==Support::InstantReadout?"instant_readout":s==Support::Unsupported?"unsupported":s==Support::DerivedLater?"derived_later":"adapter_missing";}
inline const char* typeName(FieldType t){switch(t){case FieldType::Boolean:return "bool";case FieldType::Real:return "real";case FieldType::Unsigned:return "unsigned";case FieldType::State:return "enum";case FieldType::Protection:return "text";}return "unknown";}
inline bool validRoot(const char* root,std::size_t n) {
 if(!root||!n||n>rootLimit||root[0]=='/'||root[n-1]=='/')return false;
 for(std::size_t i=0;i<n;++i) {
  const unsigned char c=static_cast<unsigned char>(root[i]);
  // A conservative ASCII prefix also keeps capabilities JSON unambiguous.
  if(c<=32||c>=127||c=='+'||c=='#'||c=='"'||c=='\\'||(c=='/'&&i&&root[i-1]=='/'))return false;
 }
 return true;
}
inline bool buildTopic(const char* root,const char* suffix,char* out,std::size_t capacity) {
 if(!out||!capacity||!root||!suffix)return false;
 out[0]='\0';const auto n=std::strlen(root),s=std::strlen(suffix);
 if(!validRoot(root,n)||!s||n+1+s>topicLimit||n+1+s>=capacity)return false;
 std::memcpy(out,root,n);out[n]='/';std::memcpy(out+n+1,suffix,s+1);return true;
}
struct Value { bool valid=false; double number=0; };
inline bool formatProtection(uint16_t bits,char* out,std::size_t capacity){
 if(!out||!capacity)return false;out[0]='\0';
 constexpr const char* names[]={"cell_overvoltage","cell_undervoltage","pack_overvoltage","pack_undervoltage",
 "charge_overtemperature","charge_undertemperature","discharge_overtemperature","discharge_undertemperature",
 "charge_overcurrent","discharge_overcurrent","short_circuit","front_end_error","software_mos_lock","charge_timeout","unknown_14","unknown_15"};
 if(!bits){if(capacity<5)return false;std::memcpy(out,"none",5);return true;}
 std::size_t used=0;
 for(unsigned i=0;i<16;++i)if(bits&(1u<<i)){
  const auto length=std::strlen(names[i]);if(used+length+(used?2:0)>=capacity){out[0]='\0';return false;}
  if(used){out[used++]=',';out[used++]=' ';}std::memcpy(out+used,names[i],length+1);used+=length;
 }
 return true;
}
inline bool formatValue(const Channel& c,Value v,char* out,std::size_t capacity) {
 if(!out||!capacity)return false;
 int n=0;
 if(!v.valid||!std::isfinite(v.number)||(c.support==Support::Unsupported||c.support==Support::DerivedLater))n=std::snprintf(out,capacity,"null");
 else switch(c.type) {
  case FieldType::Boolean:n=std::snprintf(out,capacity,v.number==0?"false":v.number==1?"true":"null");break;
  case FieldType::Real:n=std::snprintf(out,capacity,"%.7g",v.number);break;
  case FieldType::Unsigned:
   if(v.number<0||v.number>4294967295.0||std::floor(v.number)!=v.number)n=std::snprintf(out,capacity,"null");
   else n=std::snprintf(out,capacity,"%lu",static_cast<unsigned long>(v.number));break;
  case FieldType::Protection:
   if(v.number<0||v.number>65535||std::floor(v.number)!=v.number)n=std::snprintf(out,capacity,"null");
   else return formatProtection(static_cast<uint16_t>(v.number),out,capacity);break;
  case FieldType::State:
   if(v.number<0||v.number>=9||std::floor(v.number)!=v.number)n=std::snprintf(out,capacity,"null");
   else n=std::snprintf(out,capacity,"\"%s\"",chargeStates[static_cast<unsigned>(v.number)]);break;
 }
 return n>=0&&static_cast<std::size_t>(n)<capacity&&static_cast<std::size_t>(n)<=payloadLimit;
}
inline bool capabilities(const char* root,char* out,std::size_t capacity,bool victronAvailable=false,bool bmsAvailable=false,bool controlsEnabled=false) {
 char a[topicLimit+1],b[topicLimit+1],c[topicLimit+1];
 if(!buildTopic(root,commandSuffixes[0],a,sizeof(a))||!buildTopic(root,commandSuffixes[1],b,sizeof(b))||!buildTopic(root,commandSuffixes[2],c,sizeof(c)))return false;
 const int n=std::snprintf(out,capacity,
  "{\"schema_version\":1,\"read_only\":%s,\"commands_enabled\":%s,\"reserved_commands\":[\"%s\",\"%s\",\"%s\"],\"source_state\":\"%s\",\"reason\":\"%s\",\"adapters\":{\"bms\":\"jbd_readout\",\"victron\":\"instant_readout\"},\"charge_states\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]}",
  controlsEnabled?"false":"true",controlsEnabled?"true":"false",a,b,c,bmsAvailable&&victronAvailable?"available":bmsAvailable||victronAvailable?"partial":"unavailable",bmsAvailable&&victronAvailable?"ok":bmsAvailable?"victron_unavailable":victronAvailable?"bms_unavailable":"no_fresh_data",chargeStates[0],chargeStates[1],chargeStates[2],chargeStates[3],chargeStates[4],chargeStates[5],chargeStates[6],chargeStates[7],chargeStates[8]);
 return n>=0&&static_cast<std::size_t>(n)<capacity&&static_cast<std::size_t>(n)<=payloadLimit;
}
}
