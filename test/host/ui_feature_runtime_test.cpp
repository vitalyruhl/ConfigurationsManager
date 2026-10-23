#include "CompactHostString.h"
#include "runtime/RuntimeManager.h"
#include <cassert>
#include <iostream>

// [MOCKED!] Arduino String only; registry and alarm code are production code.
int main() {
  static_assert(CM_ENABLE_LIVE_UI == 0, "This test exercises Live exclusion.");
  ConfigManagerRuntime runtime;
  int calls = 0;
  runtime.addRuntimeProvider("discarded", [&](JsonObject&) { ++calls; });
  auto owned = std::make_shared<int>(1);
  std::weak_ptr<int> retained = owned;
  runtime.addRuntimeProvider("discarded-owner", [owned](JsonObject&) {});
  runtime.registerRuntimeButton("discarded", "owner", [owned] {});
  owned.reset();
  assert(retained.expired());
  RuntimeFloatMeta meta;
  meta.base.key = "discarded";
  assert(!runtime.addRuntimeMeta(meta));
  assert(!runtime.hasRuntimeMeta());
  assert(runtime.findRuntimeMeta("discarded", "discarded") == nullptr);
  assert(!runtime.runtimeMetaJsonPayload());
  runtime.defineRuntimeButton("discarded", "button", "Button", [&] { ++calls; });
  runtime.handleButtonPress("discarded", "button");
  assert(calls == 0);
  bool active = false;
  int triggered = 0, cleared = 0;
  runtime.addRuntimeAlarm("operational", [&] { return active; }, [&] {
    assert(runtime.isRuntimeAlarmActive("operational"));
    runtime.registerRuntimeAlarm("reentrant");
    ++triggered;
  }, [&] { assert(!runtime.isRuntimeAlarmActive("operational")); ++cleared; });
  active = true;
  runtime.updateAlarms();
  runtime.updateAlarms();
  assert(triggered == 1 && runtime.hasActiveAlarms());
  active = false;
  runtime.updateAlarms();
  assert(cleared == 1 && !runtime.hasActiveAlarms());
  runtime.registerRuntimeAlarm("manual", [&] { ++triggered; }, [&] { ++cleared; });
  runtime.setRuntimeAlarmActive("manual", true);
  assert(runtime.isRuntimeAlarmActive("manual") && triggered == 2);
  runtime.setRuntimeAlarmActive("manual", false);
  assert(cleared == 2);
  std::cout << "Live excluded; operational alarms and callbacks preserved. Runtime bytes=" << sizeof(runtime) << '\n';
}
