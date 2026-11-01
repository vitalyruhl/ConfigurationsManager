#include "bms/JbdProtocol.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace bms;
// Synthetic public fixtures; no captured peer data or credentials.
static std::vector<uint8_t> frame(uint8_t reg,std::vector<uint8_t> p){
 std::vector<uint8_t> f{0xdd,reg,0,static_cast<uint8_t>(p.size())};
 f.insert(f.end(),p.begin(),p.end());uint16_t sum=p.size();for(auto b:p)sum+=b;
 sum=uint16_t(0-sum);f.push_back(sum>>8);f.push_back(sum&255);f.push_back(0x77);return f;
}
static void put(std::vector<uint8_t>& p,size_t n,uint16_t v){p[n]=v>>8;p[n+1]=v&255;}
static void near(double a,double b){assert(std::fabs(a-b)<0.000001);}
static size_t channel(const char* suffix){
 for(size_t i=0;i<telemetry::channelCount;++i)if(!std::strcmp(telemetry::channels[i].suffix,suffix))return i;
 assert(false && "Required telemetry channel missing");return 0;
}
int main(){
 uint8_t command[7]{};assert(readCommand(3,command));
 const uint8_t expected[]={0xdd,0xa5,3,0,0xff,0xfd,0x77};assert(!std::memcmp(command,expected,7));
 assert(readCommand(4,command)&&command[5]==0xfc);assert(!readCommand(0xe1,command));assert(!readCommand(5,command));
 std::vector<uint8_t> basic(27);put(basic,0,4722);put(basic,2,uint16_t(-920));put(basic,4,140);put(basic,6,13125);
 put(basic,16,0x123);basic[19]=1;basic[20]=3;basic[21]=14;basic[22]=2;put(basic,23,2879);put(basic,25,2862);
 std::vector<uint8_t> cells(28);for(size_t i=0;i<14;++i)put(cells,i*2,3300+i*10);
 auto a=frame(3,basic),b=frame(4,cells);Parser parser;Frame decoded{};
 for(size_t split=0;split<a.size();++split){parser.reset();int seen=0;
  for(size_t i=0;i<split;++i)seen+=parser.push(a[i],decoded);
  assert(seen==0);for(size_t i=split;i<a.size();++i)seen+=parser.push(a[i],decoded);assert(seen==1&&decoded.reg==3);
 }
 Cycle cycle;cycle.reset();assert(cycle.accept(decoded,100));assert(!cycle.snapshot(100).available);
 for(auto v:b)if(parser.push(v,decoded))assert(cycle.accept(decoded,110));
 auto s=cycle.snapshot(110);assert(s.available);near(s.values[3].number,47.22);near(s.values[4].number,-9.2);near(s.values[5].number,-434.424);
 assert(s.cellsValid);for(size_t i=0;i<14;++i)assert(s.cellsMv[i]==3300+i*10);
 // A rejected/partial next pair cannot replace any cell of the completed sample.
 Frame partial;partial.reg=3;partial.size=27;std::copy(basic.begin(),basic.end(),partial.payload.begin());assert(cycle.accept(partial,120));
 partial.reg=4;partial.size=26;assert(!cycle.accept(partial,121));
 assert(cycle.snapshot(121).cellsMv==s.cellsMv&&cycle.snapshot(121).sequence==s.sequence);
 cycle.interrupt();partial.reg=4;partial.size=28;assert(!cycle.accept(partial,122));
 assert(!cycle.snapshot(30110).cellsValid);
 assert(s.values[0].number==0&&s.values[1].number==1);near(s.values[6].number,1.4);near(s.values[7].number,131.25);
 near(s.values[10].number,14.8);near(s.values[11].number,3.3);near(s.values[12].number,3.43);near(s.values[13].number,.13);assert(s.values[14].number==0x123);
 const auto minimum=channel("bms/temperature_min_c"), protection=channel("bms/protection");
 assert(s.values[minimum].valid);near(s.values[minimum].number,13.1);
 char text[512]{};
 assert(telemetry::formatValue(telemetry::channels[protection],{true,0},text,sizeof(text))&&!std::strcmp(text,"none"));
 assert(telemetry::formatValue(telemetry::channels[protection],{true,0xc401},text,sizeof(text)));
 assert(std::strstr(text,"cell_overvoltage")&&std::strstr(text,"short_circuit")&&std::strstr(text,"unknown_14")&&std::strstr(text,"unknown_15"));
 assert(!telemetry::formatValue(telemetry::channels[protection],{true,0xffff},text,4));
 // [MOCKED!] Synthetic basic/cell pairs exercise NTC validation and empty arrays.
 auto readPair=[&](std::vector<uint8_t> payload){Cycle c;Parser parser2;Frame f;bool accepted=false;
  for(auto v:frame(3,payload))if(parser2.push(v,f))accepted=c.accept(f,100);
  for(auto v:b)if(parser2.push(v,f))c.accept(f,110);
  return std::make_pair(accepted,c.snapshot(110));};
 const auto ntc1=channel("bms/temperature_1_c"), ntc2=channel("bms/temperature_2_c");
 auto positioned=basic;put(positioned,23,2906);put(positioned,25,2946);auto top=readPair(positioned);
 assert(top.second.available&&top.second.values[ntc1].valid&&top.second.values[ntc2].valid);
 near(top.second.values[ntc1].number,17.5);near(top.second.values[ntc2].number,21.5);
 near(top.second.values[minimum].number,17.5);near(top.second.values[10].number,21.5);
 put(positioned,23,2946);put(positioned,25,2906);auto reverse=readPair(positioned);
 near(reverse.second.values[ntc1].number,21.5);near(reverse.second.values[ntc2].number,17.5);
 near(reverse.second.values[minimum].number,17.5);near(reverse.second.values[10].number,21.5);
 positioned[22]=1;positioned.resize(25);auto single=readPair(positioned);
 assert(single.second.available&&single.second.values[ntc1].valid&&!single.second.values[ntc2].valid);
 near(single.second.values[ntc1].number,21.5);
 auto temps=basic;put(temps,23,2681);put(temps,25,2831);auto cold=readPair(temps);
 assert(cold.first&&cold.second.available);near(cold.second.values[minimum].number,-5);near(cold.second.values[10].number,10);
 temps[22]=0;temps.resize(23);auto empty=readPair(temps);
 assert(empty.second.available&&!empty.second.values[minimum].valid&&!empty.second.values[10].valid);assert(!empty.second.values[ntc1].valid&&!empty.second.values[ntc2].valid);
 temps=basic;temps[22]=9;temps.resize(41);auto overflow=readPair(temps);assert(!overflow.first&&!overflow.second.available);
 temps=basic;put(temps,23,5000);auto invalid=readPair(temps);assert(!invalid.first&&!invalid.second.available);
 assert(cycle.snapshot(30109).available&&!cycle.snapshot(30110).available);assert(!cycle.snapshot(30110).values[ntc1].valid&&!cycle.snapshot(30110).values[ntc2].valid);cycle.reset();assert(!cycle.snapshot(111).available);
 // Cell-only, bad lengths/counts/status/checksum and truncated temperatures cannot refresh a sample.
 assert(!cycle.accept(decoded,112));
 for(size_t pos: {size_t(2),size_t(5),a.size()-1}){auto bad=a;bad[pos]^=1;parser.reset();int n=0;for(auto v:bad)n+=parser.push(v,decoded);assert(n==0);}
 basic[21]=13;auto bad=frame(3,basic);for(auto v:bad)if(parser.push(v,decoded))assert(!cycle.accept(decoded,120));
 basic[21]=14;basic[22]=3;bad=frame(3,basic);for(auto v:bad)if(parser.push(v,decoded))assert(!cycle.accept(decoded,130));
 // A rejected basic frame invalidates pending basic data from the earlier cycle.
 for(auto v:a)if(parser.push(v,decoded))assert(cycle.accept(decoded,140));
 for(auto v:bad)if(parser.push(v,decoded))assert(!cycle.accept(decoded,141));
 for(auto v:b)if(parser.push(v,decoded))assert(!cycle.accept(decoded,142));
 for(auto v:a)if(parser.push(v,decoded))assert(cycle.accept(decoded,0xfffffff0u));
 for(auto v:b)if(parser.push(v,decoded))assert(cycle.accept(decoded,0xfffffff5u));assert(cycle.snapshot(10).available);
 std::puts("JBD framing, read-only commands, decoding, cycle freshness and malformed replies passed");
}
