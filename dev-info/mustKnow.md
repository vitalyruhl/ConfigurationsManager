# Must Know reference



## Auto


```cpp
int x = 10;
int& rx = x;

auto a = rx;           // a: int   (copy, reference got lost)
decltype(auto) b = rx; // b: int&  (reference remains)

a = 99;  // does NOT change x
b = 88;  // changes x
```

## ESP - Chip Info 

https://thelastoutpostworkshop.github.io/ESPConnect/

> note : only on cromium based browsers!
