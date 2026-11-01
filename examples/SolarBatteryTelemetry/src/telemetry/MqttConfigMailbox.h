#pragma once
#include "TelemetryChannels.h"
#include <mutex>
namespace telemetry {
inline bool formatClientId(uint64_t mac,char* out,std::size_t capacity){
 const int n=std::snprintf(out,capacity,"SolarBatteryTelemetry-%012llx",static_cast<unsigned long long>(mac&0xffffffffffffULL));
 return n>=0&&static_cast<std::size_t>(n)<capacity;
}
enum class TextField { Root, Server, Username, Password, ClientId };
struct MqttConfig {
 char root[rootLimit+1]="SolarBatteryTelemetry";
 char server[129]{},username[129]{},password[129]{},clientId[65]="SolarBatteryTelemetry";
 uint16_t port=1883;uint32_t publishMs=5000,fullRefreshMs=60000;int listenMs=0;
 bool enabled=false,rootRejected=false,configRejected=false;
};
// UI owns Config storage. The network owner only receives this fixed-size copy.
class ConfigMailbox {
  // bugfix: protect access to latest_ with mutex
 std::mutex mutex_;MqttConfig latest_;uint32_t revision_=1;unsigned invalid_=0;
public:
 bool text(TextField field,const char* value,std::size_t size) {
  std::lock_guard<std::mutex> guard(mutex_);
  char* dest=nullptr;std::size_t capacity=0;
  switch(field){
   case TextField::Root:dest=latest_.root;capacity=sizeof(latest_.root);break;
   case TextField::Server:dest=latest_.server;capacity=sizeof(latest_.server);break;
   case TextField::Username:dest=latest_.username;capacity=sizeof(latest_.username);break;
   case TextField::Password:dest=latest_.password;capacity=sizeof(latest_.password);break;
   case TextField::ClientId:dest=latest_.clientId;capacity=sizeof(latest_.clientId);break;
  }
  const bool accepted=value&&size<capacity&&!std::memchr(value,0,size)&&
   (field!=TextField::Root||validRoot(value,size))&&(field!=TextField::ClientId||size>0);
  const unsigned bit=1u<<static_cast<unsigned>(field);
  if(accepted){std::memcpy(dest,value,size);dest[size]=0;invalid_&=~bit;}else invalid_|=bit;
  latest_.rootRejected=(invalid_&1u)!=0;latest_.configRejected=(invalid_&~1u)!=0;++revision_;return accepted;
 }
 void enabled(bool value){std::lock_guard<std::mutex> guard(mutex_);latest_.enabled=value;++revision_;}
 bool port(int value){std::lock_guard<std::mutex> guard(mutex_);const bool ok=value>0&&value<=65535;
  if(ok){latest_.port=static_cast<uint16_t>(value);invalid_&=~32u;}else invalid_|=32u;
  latest_.configRejected=(invalid_&~1u)!=0;++revision_;return ok;}
 void intervals(float seconds,int listenMs){std::lock_guard<std::mutex> guard(mutex_);
  latest_.publishMs=std::isfinite(seconds)&&seconds>=5?static_cast<uint32_t>(seconds>3600?3600000:seconds*1000):5000;
  latest_.listenMs=listenMs<0?0:listenMs>1000?1000:listenMs;++revision_;}
 void publishInterval(float seconds){std::lock_guard<std::mutex> guard(mutex_);
  latest_.publishMs=std::isfinite(seconds)&&seconds>=5?static_cast<uint32_t>(seconds>3600?3600000:seconds*1000):5000;++revision_;}
 void fullRefreshInterval(int seconds){std::lock_guard<std::mutex> guard(mutex_);
  latest_.fullRefreshMs=static_cast<uint32_t>(seconds<5?5:seconds>3600?3600:seconds)*1000u;++revision_;}
 void listenInterval(int value){std::lock_guard<std::mutex> guard(mutex_);latest_.listenMs=value<0?0:value>1000?1000:value;++revision_;}
 bool take(uint32_t& seen,MqttConfig& out){std::lock_guard<std::mutex> guard(mutex_);
  if(seen==revision_)return false;out=latest_;seen=revision_;return true;}
};
}
