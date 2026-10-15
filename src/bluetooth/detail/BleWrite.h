#pragma once
#include <cstddef>
#include <cstdint>
namespace cm { namespace detail {
template<class Characteristic> bool writeCharacteristic(Characteristic* target,const uint8_t* data,size_t size) {
 if(!target||!data||!size||(!target->canWrite()&&!target->canWriteNoResponse()))return false;
 return target->writeValue(data,size,target->canWrite());
}
}}
