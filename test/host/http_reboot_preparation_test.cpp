#include "ota/detail/HttpRebootPreparation.h"
#include <cassert>
#include <iostream>
int main() {
  using P = cm::detail::HttpRebootPreparation;
  P policy;
  unsigned polls = 0;
  auto notReady = [&] {++polls;return false; };
  assert(policy.poll(1000, notReady) == P::Result::Idle && polls == 0);
  policy.begin(1000);
  assert(policy.poll(999, notReady) == P::Result::Waiting && polls == 0);
  assert(policy.poll(1000, notReady) == P::Result::Waiting && polls == 1);
  policy.begin(9000); // Repeated requests cannot extend preparation's deadline.
  assert(policy.poll(10999, notReady) == P::Result::Waiting);
  assert(policy.poll(11000, [] { return true; }) == P::Result::TimedOut && policy.blocked());
  assert(policy.poll(11001, [] { return true; }) == P::Result::Idle);
  policy.begin(12000);
  assert(policy.blocked());
  P ready;
  ready.begin(0xffffff00u);
  assert(ready.poll(0xf4u, [] { return true; }) == P::Result::Ready);
  assert(ready.poll(0xf5u, [] { return true; }) == P::Result::Idle);
  P wrap;
  wrap.begin(0xfffffff0u);
  assert(wrap.poll(0x26ffu, notReady) == P::Result::Waiting);
  assert(wrap.poll(0x2700u, notReady) == P::Result::TimedOut);
  std::cout << "[MOCKED!] HTTP prepare wait/ready/terminal10s/wrap passed\n";
}
