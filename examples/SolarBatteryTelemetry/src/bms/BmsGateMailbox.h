#pragma once
#include "BmsCommandPolicy.h"
namespace bms {
// Caller holds the readout mutex. Only the latest explicit setting request survives.
class GateMailbox {
 MosRequest request_{};uint32_t receivedAt_=0;bool pending_=false;
public:
 void offer(const MosRequest& request,uint32_t now){request_=request;receivedAt_=now;pending_=true;}
 bool take(uint32_t now,MosRequest& request){if(!pending_||uint32_t(now-receivedAt_)<150)return false;request=request_;pending_=false;return true;}
 void cancel(){pending_=false;}
 bool pending()const{return pending_;}
};
}
