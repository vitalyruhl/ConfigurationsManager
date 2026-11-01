#pragma once
#include "telemetry/TelemetryChannels.h"
#include "AbortDetails.h"
#include <cstdarg>
#include <cstdint>
#include <cstddef>

namespace sbt::diagnostics {
inline constexpr std::size_t mqttBufferBytes = 1536;
// PubSubClient reserves a maximum five-byte header plus a two-byte topic length.
static_assert(5 + 2 + telemetry::topicLimit + telemetry::payloadLimit <= mqttBufferBytes,
              "The complete panic PUBLISH must fit the existing MQTT buffer");
struct PanicSummary {
 AbortDetails abortDetails;
 int imageError=0,checkError=0,summaryError=0;
 bool imagePresent=false,boundsValid=false,checkAttempted=false,summaryAttempted=false;
 uint32_t imageAddress=0,imageSize=0,pc=0,cause=0,depth=0;
 bool backtraceCorrupted=false;
 uint8_t elfSha256Chars=64;
 char task[16]{};char elfSha256[65]{};uint32_t backtrace[16]{};
};

// Fixed-capacity JSON writer. Failure clears the payload instead of publishing partial JSON.
class SummaryWriter {
 char* out_;std::size_t capacity_,used_=0;bool good_;
public:
 SummaryWriter(char* out,std::size_t capacity):out_(out),capacity_(capacity),good_(out&&capacity){if(good_)out_[0]=0;}
 void append(const char* format,...){
  if(!good_)return;
  va_list args;va_start(args,format);const int n=std::vsnprintf(out_+used_,capacity_-used_,format,args);va_end(args);
  if(n<0||static_cast<std::size_t>(n)>=capacity_-used_){good_=false;out_[0]=0;return;}
  used_+=static_cast<std::size_t>(n);
 }
 void text(const char* value,std::size_t bound){
  append("\"");
  for(std::size_t i=0;i<bound&&value[i];++i){
   const unsigned char c=static_cast<unsigned char>(value[i]);
   if(c=='"'||c=='\\')append("\\%c",c);
   else if(c<32||c>=127)append("\\u%04x",static_cast<unsigned>(c));
   else append("%c",c);
  }
  append("\"");
 }
 bool finish(){if(used_>telemetry::payloadLimit){good_=false;if(out_&&capacity_)out_[0]=0;}return good_;}
};

inline bool formatPanicSummary(const PanicSummary& s,char* out,std::size_t capacity){
 SummaryWriter w(out,capacity);
 const bool valid=s.imagePresent&&s.boundsValid&&s.imageError==0&&s.checkAttempted&&s.checkError==0;
 const bool decoded=valid&&s.summaryAttempted&&s.summaryError==0;
 w.append("{\"kind\":\"historical_stored_panic\",\"dump_present\":%s,\"dump_valid\":%s,\"bounds_valid\":%s,\"image_error\":%d,\"check_error\":",
          s.imagePresent?"true":"false",valid?"true":"false",s.boundsValid?"true":"false",s.imageError);
 if(s.checkAttempted)w.append("%d",s.checkError);else w.append("null");
 w.append(",\"summary_error\":");if(s.summaryAttempted)w.append("%d",s.summaryError);else w.append("null");
 if(s.imagePresent)w.append(",\"image_address\":%lu,\"image_size\":%lu",static_cast<unsigned long>(s.imageAddress),static_cast<unsigned long>(s.imageSize));
 if(decoded){
  const unsigned hashChars=s.elfSha256Chars;
  bool hashValid=hashChars>0&&hashChars<=64;
  if(hashValid)hashValid=s.elfSha256[hashChars]==0;
  for(unsigned i=0;i<hashChars&&i<64;++i){const char c=s.elfSha256[i];hashValid=hashValid&&((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'));}
  w.append(",\"elf_sha256_valid\":%s,\"elf_sha256\":",hashValid?"true":"false");
  if(hashValid)w.text(s.elfSha256,hashChars);else w.append("null");
  w.append(",\"elf_sha256_hex_chars\":%u,\"elf_sha256_complete\":%s",hashChars,hashValid&&hashChars==64?"true":"false");
  w.append(",\"task\":");w.text(s.task,sizeof(s.task));
  const unsigned depth=s.depth>16?16:static_cast<unsigned>(s.depth);
  w.append(",\"pc\":\"0x%08lx\",\"cause\":%lu,\"bt_depth\":%u,\"bt_truncated\":%s,\"bt_corrupted\":%s,\"backtrace\":[",
           static_cast<unsigned long>(s.pc),static_cast<unsigned long>(s.cause),depth,s.depth>16?"true":"false",s.backtraceCorrupted?"true":"false");
  for(unsigned i=0;i<depth;++i)w.append("%s\"0x%08lx\"",i?",":"",static_cast<unsigned long>(s.backtrace[i]));
  w.append("]");
 }
 w.append(",\"abort_status\":\"%s\",\"abort_caller_pc\":",s.abortDetails.status);
 if(decoded&&s.abortDetails.valid)w.append("\"0x%08lx\"",static_cast<unsigned long>(s.abortDetails.callerPc));else w.append("null");
 w.append(",\"abort_core\":");if(decoded&&s.abortDetails.valid)w.append("%u",static_cast<unsigned>(s.abortDetails.core));else w.append("null");
 w.append("}");return w.finish();
}
}
