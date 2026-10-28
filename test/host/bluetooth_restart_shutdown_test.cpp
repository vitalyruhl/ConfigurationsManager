#include "bluetooth/BluetoothManager.h"
#include <cassert>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  using namespace cm;
  using namespace ble_host;
  const std::string scenario = argc > 1 ? argv[1] : "success";
  static BluetoothManager manager;
  ConfigManagerClass config;
  manager.attach(config);
  manager.settings().enabled.set(true);
  if (scenario == "workerless" || scenario == "workerless-active" || scenario == "workerfailed" || scenario == "otherowner" || scenario == "workerless-native" || scenario == "workerless-host") {
    static BluetoothManager other;
    if (scenario == "otherowner") {
      other.attach(config);
      assert(other.begin());
    }
    if (scenario == "workerless-active") {
      nativeControllerStatus = ESP_BT_CONTROLLER_STATUS_ENABLED;
      NimBLEDevice::initialized = true;
    }
    if (scenario == "workerless-native")
      nativeControllerStatus = ESP_BT_CONTROLLER_STATUS_ENABLED;
    if (scenario == "workerless-host")
      NimBLEDevice::initialized = true;
    if (scenario == "workerfailed") {
      createFailure = true;
      assert(!manager.begin());
    }
    manager.requestShutdownForRestart();
    const bool safe = scenario == "workerless" || scenario == "workerfailed";
    assert(manager.isShutdownForRestartComplete() == safe && manager.isShutdownForRestartFailed() != safe);
    assert(NimBLEDevice::deinitCalls == 0 && !manager.begin());
    return 0;
  }
  if (scenario == "idleworker-native" || scenario == "disabledworker") {
    manager.settings().enabled.set(false);
    assert(manager.begin());
    if (scenario == "idleworker-native")
      nativeControllerStatus = ESP_BT_CONTROLLER_STATUS_ENABLED;
    manager.requestShutdownForRestart();
    delay = [&] {assert(manager.isShutdownForRestartComplete()==(scenario=="disabledworker"));assert(NimBLEDevice::deinitCalls==0 && NimBLEDevice::initCalls==0);throw StopWorker{}; };
    try {
      entry(entryValue);
    } catch (const StopWorker&) {
      return 0;
    }
    return 1;
  }
  BleAddress peer{"AA:BB:CC:DD:EE:FF", 0};
  assert(manager.configureGattPeer(peer, 1, 2, 3));
  assert(manager.configureAdvertisementPeer(peer));
  assert(manager.begin());
  NimBLEDevice::deinitResult = scenario != "failure";
  NimBLEDevice::nativeStaysEnabled = scenario == "native";
  NimBLEDevice::hostStaysInitialized = scenario == "host";
  NimBLEDevice::cleanupResult = scenario != "cleanup";
  const bool success = scenario == "success" || scenario == "paused";
  unsigned pass = 0, delivered = 0;
  uint32_t oldGeneration = 0;
  manager.setEventHandler([&](const BleEvent& event) {if(event.type==BleEventType::Notification)++delivered; });
  delay = [&] {
    ++pass;
    manager.loop(now);
    if (pass == 1)
      assert(manager.requestConnect());
    if (pass == 2) {
      assert(NimBLEDevice::initCalls == 1);
      if (scenario == "paused")
        manager.requestPause(BlePauseReason::Ota);
      oldGeneration = manager.generation();
      assert(manager.configureAdvertisementPeer(peer)); // Leave pending config at shutdown.
      manager.requestShutdownForRestart();
      manager.requestShutdownForRestart();
      assert(manager.generation() != oldGeneration && !manager.isShutdownForRestartComplete());
      manager.requestResume(BlePauseReason::Ota);
      assert(manager.isPaused());
      assert(!manager.requestConnect() && !manager.clearGattPeer());
    }
    if (pass == 3) {
      assert(NimBLEDevice::stopCalls == 1);
      assert(manager.isShutdownForRestartComplete() == success);
      assert(manager.isShutdownForRestartFailed() != success);
      if (scenario == "failure" || scenario == "host" || scenario == "native")
        assert(NimBLEDevice::cleanupCalls == 0 && !NimBLEDevice::objectsDeleted);
      else
        assert(NimBLEDevice::cleanupCalls == 1);
      NimBLEDevice::client.service.characteristic.notify(99);
      manager.loop(now);
      assert(delivered == 0);
    }
    if (pass == 5) {
      assert(NimBLEDevice::stopCalls == 1 && NimBLEDevice::initCalls == 1 && !manager.begin());
      assert(manager.generation() != oldGeneration && !manager.configureGattPeer(peer, 1, 2, 3));
#if CM_TEST_RESTART_PHASES
      assert(restartMarkers > 0);
#endif
      throw StopWorker{};
    }
    now += 100;
  };
  try {
    entry(entryValue);
  } catch (const StopWorker&) {
    std::cout << "[MOCKED!] Terminal worker shutdown " << scenario << " passed\n";
    return 0;
  }
  return 1;
}
