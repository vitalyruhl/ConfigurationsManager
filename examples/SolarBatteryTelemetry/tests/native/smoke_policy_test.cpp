#include "smoke/SmokePolicy.h"
#include <cassert>
#include <iostream>
// [MOCKED!] Credential lengths are synthetic; no real credentials are used.
int main() {
 using smoke::applyNetworkDefaults;
 assert(applyNetworkDefaults(true,0,0,8,12));
 assert(applyNetworkDefaults(true,33,12,8,12));
 assert(applyNetworkDefaults(true,8,64,8,12));
 assert(!applyNetworkDefaults(true,8,12,8,12)); // Preserve configured UI values.
 assert(!applyNetworkDefaults(true,32,63,8,12));
 assert(!applyNetworkDefaults(true,8,0,8,12)); // Open network matches limiter policy.
 assert(!applyNetworkDefaults(false,0,0,8,12));
 assert(!applyNetworkDefaults(true,0,0,0,12));
 assert(!applyNetworkDefaults(true,0,0,33,12));
 assert(!applyNetworkDefaults(true,0,0,8,64));
 std::cout << "Smoke provisioning checks passed\n";
}
