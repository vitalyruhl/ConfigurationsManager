// [MOCKED!] Real BLE worker, deterministic coordinator and radio boundaries.
#include "bluetooth/BluetoothManager.h"
#include <iostream>
#include <stdexcept>
#include <string>
static void require(bool value, const char* why) {
  if (!value)
    throw std::runtime_error(why);
}
int main(int argc, char** argv) {
  using namespace cm;
  using namespace ble_host;
  const bool gatt = argc > 1 && std::string(argv[1]) == "gatt";
  try {
    static BluetoothManager manager;
    ConfigManagerClass config;
    manager.attach(config);
    manager.settings().enabled.set(true);
    BleAddress peer{"AA:BB:CC:DD:EE:FF", 0};
    require(gatt ? manager.configureGattPeer(peer, 1, 2, 3) : manager.configureAdvertisementPeer(peer), "configure peer");
    require(manager.begin(), "start worker");
    NimBLEDevice::beforeInit = [&] {
      require(config.coexistenceRequests == 1 && config.coexistenceReady,
              "NimBLE must not start before this lease receives loop acknowledgment");
    };
    unsigned passes = 0;
    delay = [&] {
      ++passes;
      manager.loop(now);
      if (passes <= 2) {
        require(NimBLEDevice::initCalls == 0, "No controller init while coordinator is pending");
        require(config.coexistenceRequests == 1, "Lease request is queued once, not busy-looped");
      }
      if (passes == 2) {
        config.coexistenceReady = true;
        if (gatt)
          require(manager.requestConnect(), "queue GATT after readiness");
      }
      if (passes == 3) {
        require(NimBLEDevice::initCalls == 1, "Native acknowledgment permits one controller init");
        manager.settings().enabled.set(false);
        manager.loop(now);
        manager.requestPause(BlePauseReason::Ota);
      }
      if (passes == 4) {
        require(config.coexistenceRequests == 1 && config.coexistenceReady, "Disable/pause does not release a live controller lease");
        require(NimBLEDevice::initCalls == 1, "No duplicate init");
        throw StopWorker{};
      }
      now += 10;
    };
    entry(entryValue);
  } catch (const StopWorker&) {
    std::cout << "[MOCKED!] BLE first-start coexistence acknowledgment passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 1;
}
