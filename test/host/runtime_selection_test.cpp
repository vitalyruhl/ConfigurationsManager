#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include "runtime/detail/RuntimeSelection.h"
#include "runtime/detail/RuntimeSnapshot.h"
#include "runtime/detail/RuntimeSubscriptions.h"

using cm::RuntimeSelection;
struct Provider { std::string name; std::function<void(JsonObject&)> fill; int order = 0; };
struct Control { std::string group; std::string key; std::function<int()> getter; };
int main() {
  RuntimeSelection selected;
  assert(!selected.all && !selected.contains("A"));
  assert(cm::parseRuntimeSelection("[\"A\",\"A\",\"controls\",\"alarms\"]", selected));
  assert(selected.groups.size() == 3 && selected.contains("A") && !selected.contains("a"));
  assert(!cm::parseRuntimeSelection("{}", selected));
  assert(selected.contains("A")); // Invalid replacement preserves last valid selection.
  assert(!cm::parseRuntimeSelection("[null]", selected));
  assert(!cm::parseRuntimeSelection("[\"\"]", selected));
  assert(!cm::parseRuntimeSelection("[\"a\\u0000b\"]", selected));
  assert(!cm::parseRuntimeSelection("[\"" + std::string(65, 'x') + "\"]", selected));
  assert(!cm::parseRuntimeSelection(std::string(2049, ' '), selected));
  JsonDocument tooMany;
  auto array = tooMany.to<JsonArray>();
  for (int i=0; i<33; ++i) array.add(std::to_string(i));
  assert(!cm::parseRuntimeSelection(array, selected));
  RuntimeSelection empty;
  assert(cm::parseRuntimeSelection("[]", empty) && !empty.all && empty.groups.empty());
  RuntimeSelection full = RuntimeSelection::everything();
  assert(full.contains("anything"));
  RuntimeSelection other;
  assert(cm::parseRuntimeSelection("[\"B\"]", other));
  auto combined = selected;
  combined.merge(other);
  assert(combined.contains("A") && combined.contains("B"));

  cm::RuntimeSubscriptions clients;
  assert(clients.connect(1, "none", 0));
  assert(!clients.connect(2, "invalid", 0));
  assert(clients.connect(2, "selected", 0));
  assert(clients.connect(3, "selected", 0));
  assert(!clients.unionSelection().all && clients.unionSelection().groups.empty());
  assert(clients.subscribe(2, "{\"type\":\"runtimeSubscribe\",\"groups\":[\"A\"]}", 1));
  assert(clients.subscribe(3, "{\"type\":\"runtimeSubscribe\",\"groups\":[\"B\",\"A\"]}", 1));
  assert(clients.unionSelection().groups.size()==2);
  assert(!clients.subscribe(2, "{\"type\":\"runtimeSubscribe\",\"groups\":null}", 1));
  assert(clients.unionSelection().contains("A"));
  assert(!clients.subscribe(1, "{\"type\":\"runtimeSubscribe\",\"groups\":[\"A\"]}", 1));
  clients.remove(3);
  assert(!clients.unionSelection().contains("B"));
  assert(clients.connect(4, "", 0) && clients.unionSelection().all);
  for (unsigned i=5;i<12;++i) assert(clients.connect(i,"selected",0));
  assert(!clients.connect(12,"selected",0));
  assert(clients.connect(13,"",0));
  assert(clients.connect(14,"none",0));

  int a=0,b=0,control=0,orphan=0;
  // [MOCKED!] Providers expose synthetic values; production snapshot helpers execute them.
  std::vector<Provider> providers{{"A",[&](JsonObject& o){++a; o["v"] = 7;}}, {"B",[&](JsonObject& o){++b; o["bulk"] = std::string(400, 'x');}}};
  std::vector<Control> controls{{"A","switch",[&](){++control;return 1;}}, {"controls","orphan",[&](){++orphan;return 2;}}};
  auto run = [&](const RuntimeSelection& s) {
    JsonDocument d;
    auto root=d.to<JsonObject>(); root["uptime"]=123;
    auto ps=cm::selectRuntimeEntries(providers,s,[](const auto& p){return p.name.c_str();});
    auto cs=cm::selectRuntimeEntries(controls,s,[](const auto& c){return c.group.c_str();});
    cm::fillRuntimeProviders(root,ps);
    cm::fillRuntimeControls(root,cs);
    root["system"]["otaActive"]=true; root["system"]["otaHasPassword"]=true;
    return d;
  };
  auto small=run(selected);
  assert(a==1 && b==0 && control==1 && orphan==1);
  assert(small["controls"]["orphan"]==2);
  auto all=run(full);
  assert(a==2 && b==1 && control==2 && orphan==2);
  auto none=run(empty);
  assert(a==2 && b==1 && control==2 && orphan==2);
  assert(none["system"]["otaActive"]==true);
  auto shared=run(combined);
  JsonDocument one,two;
  cm::filterRuntimeSnapshot(one.to<JsonObject>(),shared.as<JsonObjectConst>(),selected);
  cm::filterRuntimeSnapshot(two.to<JsonObject>(),shared.as<JsonObjectConst>(),other);
  assert(a==3 && b==2 && control==3 && orphan==3);
  assert(one["B"].isNull() && two["A"].isNull());
  assert(two["system"]["otaHasPassword"]==true);
  std::cout << "full_bytes=" << measureJson(all) << " selected_bytes=" << measureJson(small) << " empty_bytes=" << measureJson(none) << '\n';
  assert(measureJson(small)<measureJson(all));
  std::cout << "Runtime selection/provider tests passed\n";
}
