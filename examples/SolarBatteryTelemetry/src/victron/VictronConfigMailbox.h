#pragma once
#include "VictronDecoder.h"
#include <mutex>
namespace victron {
struct Configuration {char mac[18]{};uint8_t key[16]{};bool macValid=false,keyValid=false;bool ready() const{return macValid&&keyValid;}};
// UI callbacks copy into bounded storage; only the application loop touches BLE state.
class ConfigMailbox {
    std::mutex mutex_;Configuration desired_;uint32_t revision_=1;
public:
    bool setMac(const char* text,std::size_t size){
        std::lock_guard<std::mutex> lock(mutex_);desired_.macValid=validMac(text,size);
        std::memset(desired_.mac,0,sizeof(desired_.mac));
        if(desired_.macValid)for(std::size_t i=0;i<17;++i){
            if(i%3==2){desired_.mac[i]=':';continue;}
            const char c=text[size==12?i-i/3:i];
            desired_.mac[i]=(c>='A'&&c<='F')?c+('a'-'A'):c;
        }
        ++revision_;return size==0||desired_.macValid;
    }
    bool setKey(const char* text,std::size_t size){
        std::lock_guard<std::mutex> lock(mutex_);std::memset(desired_.key,0,sizeof(desired_.key));
        desired_.keyValid=parseKey(text,size,desired_.key);++revision_;return size==0||desired_.keyValid;
    }
    bool take(uint32_t& seen,Configuration& out){std::lock_guard<std::mutex> lock(mutex_);if(seen==revision_)return false;seen=revision_;out=desired_;return true;}
};
}
