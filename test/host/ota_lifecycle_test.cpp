#include "ota/detail/OtaLifecycleState.h"
#include "ota/detail/OtaUploadGate.h"
#include <cassert>
#include <vector>
#include <thread>
#include <atomic>
#include <iostream>
#include <future>
int main() {
  std::vector<OtaLifecycleEvent> events;
  cm::detail::OtaLifecycleState state([&](const OtaLifecycleEvent& e) { events.push_back(e); });
  assert(state.tryStart(OtaTransport::Http));
  auto first = state.activeSessionId();
  assert(first != 0 && !state.tryStart(OtaTransport::Arduino));
  state.finish(0, OtaStage::Failed, 1); // Rejected/unauthenticated probe owns no session.
  state.finish(first + 1, OtaStage::Failed, 9);
  assert(state.activeSessionId() == first && events.size() == 1);
  state.finish(first, OtaStage::Failed, 2);
  state.finish(first, OtaStage::Aborted, 0);
  assert(events.size() == 2 && events.back().errorCode == 2);
  for (auto stage : {OtaStage::Failed, OtaStage::Aborted, OtaStage::TimedOut}) {
    assert(state.tryStart(OtaTransport::Arduino));
    auto id = state.activeSessionId();
    assert(id != first);
    state.finish(first, stage, 1);
    assert(state.activeSessionId() == id);
    state.finish(id, stage, 7);
    assert(events.back().stage == stage);
  }
  assert(state.tryStart(OtaTransport::Http));
  auto last = state.activeSessionId();
  state.finish(last, OtaStage::Succeeded, 0);
  auto count = events.size();
  state.finish(last, OtaStage::Failed, 99);
  // Production finish can emit lifecycle events; a repeated terminal event must emit none.
  // cppcheck-suppress knownConditionTrueFalse
  assert(!state.tryStart(OtaTransport::Arduino) && events.size() == count);
  // Observer registration is independently retained by the production state helper.
  unsigned observed = 0;
  cm::detail::OtaLifecycleState retained;
  retained.setObserver([&](const OtaLifecycleEvent&) { ++observed; });
  assert(retained.tryStart(OtaTransport::Http));
  retained.finish(retained.activeSessionId(), OtaStage::Aborted, 0);
  assert(observed == 2);
  cm::detail::OtaLifecycleState empty;
  assert(empty.tryStart(OtaTransport::Arduino));
  empty.finish(empty.activeSessionId(), OtaStage::Succeeded, 0);
  cm::detail::OtaLifecycleState timeout;
  assert(timeout.tryStart(OtaTransport::Http));
  auto id = timeout.activeSessionId();
  assert(!timeout.expire(id, 0x7bfu, 0xfffffff0u, 2000));
  assert(!timeout.expire(id + 1, 0x7c0u, 0xfffffff0u, 2000));
  assert(timeout.expire(id, 0x7c0u, 0xfffffff0u, 2000));
  assert(timeout.tryStart(OtaTransport::Http));
  assert(timeout.expire(timeout.activeSessionId(), 2000, 0, 2000));
  for (auto transport : {OtaTransport::Arduino, OtaTransport::Http}) {
    std::vector<OtaStage> stages;
    cm::detail::OtaLifecycleState success([&](const OtaLifecycleEvent& e) {
      assert(e.transport == transport);
      stages.push_back(e.stage);
    });
    assert(success.tryStart(transport));
    success.finish(success.activeSessionId(), OtaStage::Succeeded, 0);
    assert((stages == std::vector<OtaStage>{OtaStage::Starting, OtaStage::Succeeded}));
  }
  // Simultaneous transport admission has exactly one winner.
  cm::detail::OtaLifecycleState concurrent;
  std::atomic<unsigned> winners{0};
  std::thread a([&] { if(concurrent.tryStart(OtaTransport::Http)) ++winners; });
  std::thread b([&] { if(concurrent.tryStart(OtaTransport::Arduino)) ++winners; });
  a.join();
  b.join();
  assert(winners == 1);
  // An ordinary servicing guard cannot exclude the admitted HTTP owner.
  cm::detail::OtaLifecycleState http;
  cm::detail::OtaUploadGate gate(http);
  {
    auto admission = gate.tryRequest();
    assert(admission.owns_lock());
    assert(http.tryStart(OtaTransport::Http));
  }
  auto service = gate.tryService(false);
  std::atomic<bool> ownerChunkAccepted{false};
  std::thread chunk([&] {auto owner=gate.tryRequest();ownerChunkAccepted=owner.owns_lock(); });
  chunk.join();
  assert(ownerChunkAccepted && !service.owns_lock());
  const auto httpId = http.activeSessionId();
  {
    auto owner = gate.tryRequest();
    assert(owner.owns_lock());
    http.finish(httpId, OtaStage::Succeeded, 0);
  }
  // A successful HTTP response/reboot handoff does no flash work and must not
  // compete with ordinary servicing after the lifecycle active ID becomes zero.
  std::promise<void> serviceHeld, releaseService;
  auto release = releaseService.get_future();
  std::thread servicing([&] {
    auto poll = gate.tryService(false);
    assert(poll.owns_lock());
    serviceHeld.set_value();
    release.wait();
  });
  serviceHeld.get_future().wait();
  bool responseInstalled = false, rebootInstalled = false;
  const bool completedHttp = gate.completeHttp(httpId, [&] {responseInstalled=true;rebootInstalled=true; });
  assert(completedHttp);
  assert(responseInstalled && rebootInstalled);
  assert(!gate.completeHttp(httpId + 1, [&] { assert(false); }));
  assert(!http.tryStart(OtaTransport::Arduino));
  http.finish(httpId, OtaStage::Aborted, 0); // Late disable/cleanup cannot undo success.
  assert(gate.completeHttp(httpId, [] {}));
  releaseService.set_value();
  servicing.join();
  std::cout << "OTA lifecycle checks passed\n";
}
