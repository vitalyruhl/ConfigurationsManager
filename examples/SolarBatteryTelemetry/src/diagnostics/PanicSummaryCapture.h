#pragma once
#include "PanicSummary.h"
#include <esp_core_dump.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_private/panic_internal.h>

namespace sbt::diagnostics {
// Read once before workers start. The SDK validates CRC before decoding; no erase API is used.
// Only the SDK's bounded summary is copied, never the partition image or raw registers.
inline const PanicSummary& captureStoredPanicOnce(){
 static PanicSummary result;
 static bool captured=false;
 if(captured)return result;
 captured=true;
 size_t address=0,size=0;
 result.imageError=esp_core_dump_image_get(&address,&size);
 if(result.imageError!=ESP_OK)return result;
 result.imagePresent=true;result.imageAddress=static_cast<uint32_t>(address);result.imageSize=static_cast<uint32_t>(size);
 const auto* partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_COREDUMP,nullptr);
 result.boundsValid=partition&&address>=partition->address&&size>0&&size<=partition->size&&address-partition->address<=partition->size-size;
 if(!result.boundsValid)return result;
 result.checkAttempted=true;result.checkError=esp_core_dump_image_check();
 if(result.checkError!=ESP_OK)return result;
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
 static esp_core_dump_summary_t native{};
 result.summaryAttempted=true;result.summaryError=esp_core_dump_get_summary(&native);
 if(result.summaryError!=ESP_OK)return result;
 result.pc=native.exc_pc;result.cause=native.ex_info.exc_cause;
 result.depth=native.exc_bt_info.depth;result.backtraceCorrupted=native.exc_bt_info.corrupted;
 std::memcpy(result.task,native.exc_task,sizeof(result.task));
 static_assert(sizeof(native.app_elf_sha256)>1&&sizeof(native.app_elf_sha256)<=sizeof(result.elfSha256),"SDK ELF SHA prefix must fit");
 result.elfSha256Chars=sizeof(native.app_elf_sha256)-1;
 std::memcpy(result.elfSha256,native.app_elf_sha256,sizeof(native.app_elf_sha256));
 static_assert(sizeof(native.exc_bt_info.bt)==sizeof(result.backtrace),"Expected sixteen SDK backtrace entries");
 std::memcpy(result.backtrace,native.exc_bt_info.bt,sizeof(result.backtrace));
 char currentPrefix[17]{};esp_ota_get_app_elf_sha256(currentPrefix,sizeof(currentPrefix));
 // This pinned SDK emits the abort null-store frame at panic_abort + 0x15.
 // Foreign ELF identities never inherit that interpretation solely from a matching address.
 const bool context=knownAbortContext(result.elfSha256,result.elfSha256Chars,result.pc,currentPrefix,
     static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&panic_abort))+0x15u);
 struct Reader {
  const esp_partition_t* partition;uint32_t base;
  bool read(uint32_t at,void* out,std::size_t length){return base<=partition->size&&at<=partition->size-base&&length<=partition->size-base-at&&esp_partition_read(partition,base+at,out,length)==ESP_OK;}
 } reader{partition,static_cast<uint32_t>(address-partition->address)};
 result.abortDetails=resolveAbortDetails(reader,static_cast<uint32_t>(size),native.ex_info.exc_a[2],context);
#else
 result.summaryAttempted=true;result.summaryError=ESP_ERR_NOT_SUPPORTED;
#endif
 return result;
}
}
