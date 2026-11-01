#pragma once
#include <runtime/RuntimeFieldTypes.h>
#include <type_traits>
#include <cstdint>

namespace sil {
template<class Meta,class Runtime>
RuntimeFieldId compactField(Runtime& runtime,const char* source,const char* page,
                            const char* card,const char* group,const char* key,
                            const char* label,int order,const char* unit="",int precision=2) {
    Meta meta;
    meta.base.sourceGroup=source;meta.base.page=page;meta.base.card=card;meta.base.group=group;
    meta.base.key=key;meta.base.label=label;meta.base.order=order;
    if constexpr(std::is_same_v<Meta,RuntimeFloatMeta> || std::is_same_v<Meta,RuntimeIntMeta>) meta.unit=unit;
    if constexpr(std::is_same_v<Meta,RuntimeFloatMeta>) meta.precision=precision;
    return runtime.addRuntimeMeta(meta);
}

struct HeapSnapshot { uint32_t used, free, fragmentation; };
inline HeapSnapshot heapSnapshot(uint32_t total, uint32_t free, uint32_t largest) {
    const uint32_t boundedLargest = largest > free ? free : largest;
    return {total >= free ? total - free : 0, free,
        free ? uint32_t(100 - uint64_t(boundedLargest) * 100 / free) : 100u};
}
template<class Runtime> bool registerHeapMetadata(Runtime& runtime) {
    return static_cast<bool>(compactField<RuntimeIntMeta>(runtime,"Limiter","Limiter","Limiter","Limiter Status","usedHeap","UsedHeap",40,"B")) &&
        static_cast<bool>(compactField<RuntimeIntMeta>(runtime,"Limiter","Limiter","Limiter","Limiter Status","freeHeap","FreeHeap",41,"B")) &&
        static_cast<bool>(compactField<RuntimeIntMeta>(runtime,"Limiter","Limiter","Limiter","Limiter Status","heapFragmentation","HeapFragmentation",42,"%"));
}

// Keep the existing one-field providers and getter lifetimes while shrinking metadata.
template<class Runtime> class CompactLiveGroup {
    Runtime& runtime_;
    const char* source_;
    const char* page_;
    const char* card_;
    const char* group_;
    bool valid_=true;
public:
    CompactLiveGroup(Runtime& runtime,const char* source,const char* page,const char* card,const char* group)
        :runtime_(runtime),source_(source),page_(page),card_(card),group_(group){}
    template<class Meta,class Getter>
    void value(const char* key,Getter getter,const char* label,const char* unit,int precision,int order) {
        if(!valid_) return;
        valid_=static_cast<bool>(compactField<Meta>(runtime_,source_,page_,card_,group_,key,label,order,unit,precision));
        if(!valid_) return;
        String keyStr(key);
        runtime_.addRuntimeProvider(source_,[keyStr,getter](JsonObject& data){data[keyStr]=getter();},order);
    }
    bool valid() const {return valid_;}
};
}
