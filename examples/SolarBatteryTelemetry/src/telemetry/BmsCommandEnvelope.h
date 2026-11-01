#pragma once
#include <ArduinoJson.h>
#include "bms/BmsCommandPolicy.h"
namespace telemetry {
// Per-connection replay ledger. A full ledger requires a new advertised session.
class CommandSession {
 char token_[33]{};char ids_[16][33]{};size_t count_=0;
 static bool validId(const char* id){
  if(!id)return false;const auto n=std::strlen(id);if(!n||n>32)return false;
  for(size_t i=0;i<n;++i){char c=id[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;}return true;
 }
 // The command vocabulary needs no JSON escapes or nested objects. Reject ambiguous
 // encodings before ArduinoJson can stop early or collapse duplicate properties.
 static bool exactObject(const char* text,size_t size,unsigned expectedProperties){
  size_t first=0,last=size;auto space=[](char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};
  while(first<last&&space(text[first]))++first;while(last>first&&space(text[last-1]))--last;
  if(last-first<2||text[first]!='{'||text[last-1]!='}')return false;
  bool quoted=false;unsigned properties=0;
  for(size_t i=first+1;i+1<last;++i){const char c=text[i];
   if(c=='\\'||c==0)return false;
   if(c=='"'){quoted=!quoted;continue;}
   if(!quoted){if(c=='{'||c=='}'||c=='['||c==']')return false;if(c==':')++properties;}
  }
  return !quoted&&properties==expectedProperties;
 }
public:
 void clear(){token_[0]=0;count_=0;}
 void begin(const char* token){clear();if(token&&std::strlen(token)==32)std::memcpy(token_,token,33);}
 const char* token()const{return token_;}
 const char* accept(const char* suffix,const char* payload,size_t length,uint64_t utc,uint32_t now,bms::MosRequest& output){
  if(!suffix)return "unsupported_command";
  const bool gate=!std::strcmp(suffix,"bms/control_enabled/set");
  const bool emergency=!std::strcmp(suffix,"bms/emergency_off/set");
  const bool both=!std::strcmp(suffix,"bms/permissions/set");
  if(!both&&!gate&&!emergency&&std::strcmp(suffix,"bms/charge_enabled/set")&&std::strcmp(suffix,"bms/discharge_enabled/set"))return "unsupported_command";
  if(!payload||!length||length>256)return "payload_size";
  if(utc<1577836800)return "clock_unavailable";
  const unsigned properties=both?5u:4u;
  if(!exactObject(payload,length,properties)&&(both||!exactObject(payload,length,7)))return "invalid_envelope";
  JsonDocument doc;auto error=deserializeJson(doc,payload,length);
  if(error||!doc.is<JsonObject>()||(doc.size()!=properties&&(both||doc.size()!=7))||!doc["expires_at"].is<uint64_t>()||!doc["request_id"].is<const char*>()||!doc["session"].is<const char*>())return "invalid_envelope";
  if(both?(!doc["charge_enabled"].is<bool>()||!doc["discharge_enabled"].is<bool>()):!doc["state"].is<bool>())return "invalid_envelope";
  if((gate||emergency)&&doc.size()!=4)return "invalid_envelope";
  if(emergency&&doc["state"].as<bool>())return "emergency_off_only";
  const bool baseline=!both&&doc.size()==7;
  if(baseline){
   if(!doc["baseline_charge_enabled"].is<bool>()||!doc["baseline_discharge_enabled"].is<bool>()||!doc["peer"].is<const char*>())return "invalid_baseline";
   const char* peer=doc["peer"].as<const char*>();if(std::strlen(peer)!=17)return "invalid_peer";
   for(unsigned i=0;i<17;++i){char c=peer[i];if(i%3==2?c!=':':!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return "invalid_peer";}
  }
  const char* id=doc["request_id"].as<const char*>();const char* token=doc["session"].as<const char*>();
  if(!validId(id))return "invalid_request_id";
  if(!token_[0]||std::strcmp(token_,token))return "old_session";
  const auto expiry=doc["expires_at"].as<uint64_t>();
  if(expiry<=utc)return "expired";if(expiry-utc>30)return "expiry_too_far";
  for(size_t i=0;i<count_;++i)if(!std::strcmp(ids_[i],id))return "duplicate";
  if(count_==16)return "session_exhausted";
  std::memcpy(ids_[count_++],id,std::strlen(id)+1);
  output={};std::memcpy(output.id,id,std::strlen(id)+1);output.emergency=emergency;output.target=gate?bms::MosTarget::Gate:(both||emergency)?bms::MosTarget::Both:std::strcmp(suffix,"bms/charge_enabled/set")?bms::MosTarget::Discharge:bms::MosTarget::Charge;
  output.state=doc[both?"charge_enabled":"state"].as<bool>();
  if(both)output.dischargeState=doc["discharge_enabled"].as<bool>();
  if(baseline){
   output.baselineKnown=true;output.baselineMask=(doc["baseline_charge_enabled"].as<bool>()?1:0)|(doc["baseline_discharge_enabled"].as<bool>()?2:0);
   const char* peer=doc["peer"].as<const char*>();for(unsigned i=0;i<17;++i)output.peer[i]=peer[i]>='A'&&peer[i]<='F'?char(peer[i]-'A'+'a'):peer[i];
  }
  output.deadline=now+static_cast<uint32_t>((expiry-utc)*1000);return "ok";
 }
};
}
