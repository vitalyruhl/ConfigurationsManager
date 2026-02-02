# C++ Common `#include`s – Practical Cheat Sheet (with typical APIs)

This is a **pragmatic** overview of **commonly used** C++ standard headers (no super exotic stuff).
For each header you get:
- what it’s for
- the **most-used** types / functions / methods (not an academic 100% dump)
- a tiny example

> Note: Some headers (e.g. `<vector>`, `<string>`, `<algorithm>`) have *very large* APIs.  
> Here you’ll find the members people actually use day-to-day.

---

## Containers

## `<vector>`
Dynamic contiguous array.

**Typical API**
- `push_back`, `emplace_back`, `pop_back`
- `size`, `empty`, `clear`, `resize`, `reserve`, `capacity`, `shrink_to_fit`
- `operator[]`, `at`, `front`, `back`, `data`
- iterators: `begin/end`, `cbegin/cend`, `rbegin/rend`
- modifiers: `insert`, `erase`, `assign`, `swap`

```cpp
#include <vector>

std::vector<int> v;
v.reserve(10);
v.push_back(1);
v.emplace_back(2);

for (int x : v) { /* ... */ }

v.erase(v.begin());      // remove first element
int last = v.back();     // 2
```

---

## `<array>`
Fixed-size container (stack-friendly).

**Typical API**
- `size`, `empty`, `fill`, `swap`
- `operator[]`, `at`, `front`, `back`, `data`
- iterators: `begin/end`, `rbegin/rend`

```cpp
#include <array>

std::array<int, 3> a{1,2,3};
a.fill(7);     // [7,7,7]
int x = a[0];
```

---

## `<deque>`
Double-ended queue; fast push/pop at both ends.

**Typical API**
- `push_back/front`, `emplace_back/front`, `pop_back/front`
- `operator[]`, `at`, `front`, `back`
- `insert`, `erase`, `clear`, `size`

```cpp
#include <deque>

std::deque<int> d;
d.push_front(1);
d.push_back(2);
int first = d.front(); // 1
```

---

## `<list>`
Doubly-linked list; stable iterators on insert/erase.

**Typical API**
- `push_back/front`, `pop_back/front`
- `insert`, `erase`, `remove`, `remove_if`
- `splice`, `merge`, `sort`, `unique`

```cpp
#include <list>

std::list<int> lst{3,1,2,2};
lst.sort();
lst.unique(); // remove consecutive duplicates -> [1,2,3]
```

---

## `<map>`
Ordered key/value (tree), sorted by key.

**Typical API**
- `operator[]`, `at`, `insert`, `emplace`, `try_emplace`
- `find`, `contains` (C++20), `count`, `erase`, `clear`
- `lower_bound`, `upper_bound`, `equal_range`
- iterators: `begin/end`

```cpp
#include <map>
#include <string>

std::map<std::string, int> m;
m["a"] = 1;
m.try_emplace("b", 2);

if (auto it = m.find("a"); it != m.end()) {
    int v = it->second;
}
```

---

## `<unordered_map>`
Hash map; average O(1) lookup.

**Typical API**
- `operator[]`, `at`, `insert`, `emplace`
- `find`, `contains` (C++20), `erase`, `clear`
- bucket/rehash: `reserve`, `rehash`, `load_factor`, `max_load_factor`

```cpp
#include <unordered_map>
#include <string>

std::unordered_map<std::string, int> c;
c["x"]++;
if (c.contains("x")) { /* ... */ }
```

---

## `<set>` / `<unordered_set>`
Unique elements; ordered vs hashed.

**Typical API**
- `insert`, `emplace`, `erase`, `clear`
- `find`, `contains` (C++20), `count`

```cpp
#include <set>

std::set<int> s;
s.insert(3);
s.insert(3); // ignored
```

---

## Text & Strings

## `<string>`
Owning UTF-8 byte string (no unicode semantics, just bytes).

**Typical API**
- size: `size`, `length`, `empty`, `clear`, `resize`, `reserve`
- access: `operator[]`, `at`, `front`, `back`, `data`, `c_str`
- modify: `+=`, `append`, `push_back`, `insert`, `erase`, `replace`, `substr`
- search: `find`, `rfind`, `find_first_of`, `find_last_of`
- compare: `compare`, relational operators

```cpp
#include <string>

std::string s = "ESP32";
s += " Dev";
auto pos = s.find("32");
std::string sub = s.substr(pos, 2); // "32"
```

---

## `<string_view>`
Non-owning view into a string (cheap slicing).

**Typical API**
- `size`, `empty`, `data`
- `substr`, `remove_prefix`, `remove_suffix`
- `find`, `starts_with`, `ends_with` (C++20)

```cpp
#include <string_view>

std::string_view sv = "hello world";
sv.remove_prefix(6); // "world"
```

---

## IO

## `<iostream>`
Console IO (streams).

**Typical API**
- `std::cout`, `std::cerr`, `std::cin`
- manipulators: `std::endl`, `std::boolalpha`, `std::hex`, `std::dec`
- `operator<<`, `operator>>`

```cpp
#include <iostream>

int x = 0;
std::cin >> x;
std::cout << "x=" << x << std::endl;
```

---

## `<fstream>`
File IO.

**Typical API**
- `std::ifstream`, `std::ofstream`, `std::fstream`
- `open`, `close`, `is_open`, stream operators / `read` / `write`
- flags: `std::ios::binary`, `std::ios::app`

```cpp
#include <fstream>
#include <string>

std::ofstream out("log.txt", std::ios::app);
out << "hello
";

std::ifstream in("log.txt");
std::string line;
std::getline(in, line);
```

---

## `<sstream>`
String streams (parse/format).

**Typical API**
- `std::stringstream`, `std::istringstream`, `std::ostringstream`
- `str()`, `operator<<`, `operator>>`

```cpp
#include <sstream>
#include <string>

std::istringstream iss("12 34");
int a,b;
iss >> a >> b;
```

---

## Algorithms

## `<algorithm>`
Algorithms over iterators.

**Most-used functions**
- searching: `find`, `find_if`, `count`, `count_if`, `any_of`, `all_of`, `none_of`
- sorting: `sort`, `stable_sort`, `partial_sort`, `nth_element`, `is_sorted`
- min/max: `min`, `max`, `minmax`, `min_element`, `max_element`
- modify: `reverse`, `rotate`, `remove`, `remove_if`, `unique`, `erase` patterns
- copy/move: `copy`, `copy_if`, `transform`, `move`, `swap`
- set ops: `set_union`, `set_intersection` (for sorted ranges)

```cpp
#include <algorithm>
#include <vector>

std::vector<int> v{3,1,2,2};
std::sort(v.begin(), v.end());
v.erase(std::unique(v.begin(), v.end()), v.end()); // dedupe
```

---

## `<numeric>`
Numeric algorithms on ranges.

**Functions**
- `std::accumulate` (C++98)
- `std::reduce` (C++17)
- `std::inner_product` (C++98)
- `std::adjacent_difference` (C++98)
- `std::partial_sum` (C++98)
- scans (C++17): `std::exclusive_scan`, `std::inclusive_scan`, `std::transform_exclusive_scan`, `std::transform_inclusive_scan`
- gcd/lcm (C++17): `std::gcd`, `std::lcm`
- midpoint (C++20): `std::midpoint`

```cpp
#include <numeric>
#include <vector>

std::vector<int> v{1,2,3};
int sum = std::accumulate(v.begin(), v.end(), 0); // 6
```

---

## Utilities

## `<utility>`
Pairs and move/forward helpers.

**Typical API**
- `std::pair`, `std::make_pair`
- `std::move`, `std::forward`, `std::swap`

```cpp
#include <utility>

std::pair<int,int> p = {1,2};
auto q = std::make_pair(3,4);
```

---

## `<tuple>`
Multiple values; `std::get`, structured bindings.

**Typical API**
- `std::tuple`, `std::make_tuple`, `std::tie`
- `std::get<N>(t)`

```cpp
#include <tuple>
#include <string>

auto t = std::make_tuple(1, std::string("x"));
auto [id, name] = t;
```

---

## `<optional>`
Value-or-empty.

**Typical API**
- `has_value`, `operator bool`
- `value`, `value_or`, `reset`, `emplace`
- `operator*`, `operator->`

```cpp
#include <optional>

std::optional<int> o;
o.emplace(42);
int x = o.value_or(0);
```

---

## `<variant>`
Type-safe union.

**Typical API**
- `std::variant<T...>`
- `std::holds_alternative<T>`, `std::get<T>`, `std::get_if<T>`
- `std::visit`

```cpp
#include <variant>
#include <string>

std::variant<int, std::string> v = "hi";
if (auto p = std::get_if<std::string>(&v)) {
    // *p == "hi"
}
```

---

## `<any>`
Type-erased container (use sparingly).

**Typical API**
- `std::any`
- `has_value`, `reset`
- `std::any_cast<T>`

```cpp
#include <any>
#include <string>

std::any a = std::string("x");
auto s = std::any_cast<std::string>(a);
```

---

## Memory & Ownership

## `<memory>`
Smart pointers & allocators.

**Typical API**
- `std::unique_ptr`, `std::make_unique`
- `std::shared_ptr`, `std::make_shared`, `use_count`
- `std::weak_ptr`, `lock`, `expired`
- `std::enable_shared_from_this`
- raw helpers: `std::addressof`

```cpp
#include <memory>

auto p = std::make_unique<int>(7);
auto sp = std::make_shared<int>(9);
```

---

## Functional / Callbacks

## `<functional>`
Function wrappers and standard function objects.

**Typical API**
- `std::function`
- `std::bind` (often avoid; prefer lambdas)
- `std::ref`, `std::cref`
- function objects: `std::plus<>`, `std::less<>`, `std::multiplies<>`

```cpp
#include <functional>

std::function<int(int,int)> add = [](int a,int b){ return a+b; };
int r = add(1,2);
```

---

## Time

## `<chrono>`
Type-safe durations & time points.

**Typical API**
- clocks: `steady_clock`, `system_clock`
- duration literals (with `<chrono>` + `using namespace std::chrono_literals;`): `100ms`, `2s` (C++14)
- `duration_cast`
- `time_point`, `now()`

```cpp
#include <chrono>

auto t0 = std::chrono::steady_clock::now();
// ... work ...
auto t1 = std::chrono::steady_clock::now();
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
```

---

## `<thread>` / `<mutex>` / `<condition_variable>`
Threads & synchronization (desktop/server; embedded only if supported by runtime).

### `<thread>`
- `std::thread`, `join`, `detach`, `hardware_concurrency`, `sleep_for`, `sleep_until`

```cpp
#include <thread>
#include <chrono>

std::thread t([]{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
});
t.join();
```

### `<mutex>`
- `std::mutex`, `lock`, `unlock`, `try_lock`
- `std::lock_guard`, `std::unique_lock`

```cpp
#include <mutex>

std::mutex m;
{
    std::lock_guard<std::mutex> lk(m);
    // protected section
}
```

### `<condition_variable>`
- `std::condition_variable`, `wait`, `notify_one`, `notify_all`

```cpp
#include <condition_variable>
#include <mutex>

std::mutex m;
std::condition_variable cv;
bool ready = false;

void worker() {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, []{ return ready; });
}
```

---

## Errors & Exceptions

## `<stdexcept>`
Standard exception types.

**Types**
- `std::runtime_error`, `std::logic_error`
- `std::invalid_argument`, `std::out_of_range`

```cpp
#include <stdexcept>

throw std::runtime_error("something failed");
```

---

## C / Embedded-friendly basics

## `<cstdint>`
Fixed-width integers.

**Types**
- `uint8_t`, `uint16_t`, `uint32_t`, `int32_t`, …

```cpp
#include <cstdint>

uint8_t b = 255;
uint32_t n = 123456u;
```

---

## `<cstddef>`
Size and pointer related types.

**Types**
- `size_t`, `ptrdiff_t`, `nullptr_t`

```cpp
#include <cstddef>

size_t n = 10;
```

---

## `<cmath>`
Math functions.

**Functions**
- `std::sqrt`, `std::sin`, `std::cos`, `std::pow`, `std::abs`, `std::floor`, `std::ceil`

```cpp
#include <cmath>

double r = std::sqrt(9.0); // 3.0
```

---

## `<cstring>`
C-string functions.

**Functions**
- `std::strlen`, `std::strcmp`, `std::strncpy`, `std::memcpy`, `std::memset`

```cpp
#include <cstring>

char buf[32]{};
std::memset(buf, 0, sizeof(buf));
```

---

