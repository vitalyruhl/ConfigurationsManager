#include "runtime/detail/LivePageSelection.h"
#include <cassert>
#include <string>
#include <vector>
#include <iostream>
struct Placement { std::string id,page,sourceGroup; };
int main() {
  assert(cm::validLivePageSelector("my page!",8));
  assert(!cm::validLivePageSelector("",0));
  const char nul[]={'a',0,'b'};
  assert(!cm::validLivePageSelector(nul,3));
  assert(!cm::validLivePageSelector(std::string(257,'a').c_str(),257));
  std::vector<Placement> placements={{"reason","BMS","bms"},{"reason","Victron","victron"},{"availability","BMS","bms"},{"availability","Victron","victron"}};
  assert(cm::findLivePlacement(placements,"reason","bms")->page=="BMS");
  assert(cm::findLivePlacement(placements,"reason","victron")->page=="Victron");
  assert(cm::findLivePlacement(placements,"availability","bms")->page=="BMS");
  assert(cm::findLivePlacement(placements,"reason","BMS")==nullptr);
  assert(!cm::sameLiveField(placements[0],"reason","victron"));
  assert(cm::sameLiveField(placements[0],"reason","bms"));
  placements.push_back({"reason","Explicit Override",""});
  assert(cm::findLivePlacement(placements,"reason","bms")->page=="Explicit Override");
  assert(cm::findLivePlacement(placements,"reason","victron")->page=="Explicit Override");
  assert(cm::findLivePlacement(placements,"availability","victron")->page=="Victron");
  std::cout<<"Page selection passed: source+key isolation, exact keys, bounded selectors and explicit legacy override\n";
}
