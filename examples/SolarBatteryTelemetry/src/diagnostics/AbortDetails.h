#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace sbt::diagnostics {
struct AbortDetails {const char* status="not_attempted";uint32_t callerPc=0;uint8_t core=0;bool valid=false;};
inline uint16_t abortLe16(const uint8_t* p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline uint32_t abortLe32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
// Pinned classic ESP32 SDK: 20-byte outer header, CRC32 footer, ELF32 little endian.
// Verified against a saved real COM8 image. Reader offsets refer only to that image.
template<class Reader> AbortDetails resolveAbortDetails(Reader& reader,uint32_t imageSize,uint32_t pointer,bool knownContext){
 auto failure=[](const char* status){AbortDetails out;out.status=status;return out;};
 if(!knownContext)return failure("context");
 if(imageSize<76||imageSize>65536)return failure("size");
 auto read=[&](uint32_t at,void* out,std::size_t n){return at<=imageSize&&n<=imageSize-at&&reader.read(at,out,n);};
 uint8_t outer[20],elf[52];
 if(!read(0,outer,sizeof(outer))||!read(20,elf,sizeof(elf)))return failure("read");
 if(abortLe32(outer)!=imageSize||abortLe32(outer+4)!=0x100)return failure("header");
 if(std::memcmp(elf,"\x7f" "ELF",4)||elf[4]!=1||elf[5]!=1||elf[6]!=1||abortLe16(elf+16)!=4||abortLe16(elf+18)!=94||abortLe32(elf+20)!=1||abortLe16(elf+40)!=52||abortLe16(elf+42)!=32)return failure("elf");
 const uint32_t elfSize=imageSize-24,phoff=abortLe32(elf+28),count=abortLe16(elf+44);
 if(!count||count>160||phoff<52||phoff>elfSize||count>(elfSize-phoff)/32)return failure("phdr");
 const uint32_t headerEnd=phoff+count*32;
 uint32_t selected=UINT32_MAX,fileOffset=0,available=0;uint8_t ph[32];
 for(uint32_t i=0;i<count;++i){
  if(!read(20+phoff+i*32,ph,sizeof(ph)))return failure("read");
  const uint32_t offset=abortLe32(ph+4),address=abortLe32(ph+8),size=abortLe32(ph+16),memory=abortLe32(ph+20);
  if(offset>elfSize||size>elfSize-offset)return failure("segment");
  if(abortLe32(ph)!=1)continue;
  if(size>memory||memory>UINT32_MAX-address||(size&&offset<headerEnd))return failure("segment");
  if(pointer>=address&&pointer-address<size){
   if(selected!=UINT32_MAX)return failure("overlap");
   selected=i;fileOffset=offset+(pointer-address);available=size-(pointer-address);
  }
 }
 if(selected==UINT32_MAX)return failure("unmapped");
 const uint32_t length=available<64?available:64;
 // Reject any competing virtual mapping inside the small prospective string window.
 for(uint32_t i=0;i<count;++i){
  if(i==selected)continue;
  if(!read(20+phoff+i*32,ph,sizeof(ph)))return failure("read");
  if(abortLe32(ph)!=1)continue;
  const uint32_t address=abortLe32(ph+8),size=abortLe32(ph+16);
  if(size&&address<pointer+length&&pointer<address+size)return failure("overlap");
 }
 char text[64];if(!read(20+fileOffset,text,length))return failure("read");
 std::size_t n=0;while(n<length&&text[n])++n;
 if(n==length)return failure("no_nul");
 constexpr char prefix[]="abort() was called at PC 0x",suffix[]=" on core ";
 constexpr std::size_t prefixSize=sizeof(prefix)-1,suffixSize=sizeof(suffix)-1;
 if(n!=prefixSize+8+suffixSize+1||std::memcmp(text,prefix,prefixSize)||std::memcmp(text+prefixSize+8,suffix,suffixSize))return failure("format");
 uint32_t pc=0;for(unsigned i=0;i<8;++i){const char c=text[prefixSize+i];unsigned digit;if(c>='0'&&c<='9')digit=c-'0';else if(c>='a'&&c<='f')digit=c-'a'+10;else return failure("format");pc=(pc<<4)|digit;}
 const char core=text[n-1];if((core!='0'&&core!='1')||pc<0x40000000||pc>=0x40400000)return failure("format");
 AbortDetails out;out.status="ok";out.callerPc=pc;out.core=static_cast<uint8_t>(core-'0');out.valid=true;return out;
}

inline bool knownAbortContext(const char* storedPrefix,std::size_t chars,uint32_t pc,const char* currentPrefix,uint32_t currentAbortFaultPc){
 if(chars!=16||storedPrefix[16]!=0)return false;
 for(unsigned i=0;i<16;++i)if(!((storedPrefix[i]>='0'&&storedPrefix[i]<='9')||(storedPrefix[i]>='a'&&storedPrefix[i]<='f')))return false;
 // Exact archived 1.4.8 and normal 1.4.9 ELF identities and inspected abort instruction.
 if(pc==0x40083855&&(!std::memcmp(storedPrefix,"35d6636347191fec",16)||!std::memcmp(storedPrefix,"fcd49c79dac3bfb7",16)))return true;
 return currentPrefix&&currentPrefix[16]==0&&!std::memcmp(storedPrefix,currentPrefix,16)&&pc==currentAbortFaultPc;
}
}
