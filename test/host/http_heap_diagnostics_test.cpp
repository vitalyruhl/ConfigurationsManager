#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "web/HttpHeapDiagnostics.h"
static unsigned allocations = 0;
void* operator new(size_t size) { ++allocations; if (void* p=std::malloc(size ? size : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
#if CM_ENABLE_VERBOSE_LOGGING
static char captured[320];
static unsigned queries=0, checks=0, logs=0;
static bool intact=true;
void heap_caps_get_info(multi_heap_info_t* info,uint32_t caps) {
  assert(caps==MALLOC_CAP_DEFAULT); ++queries;
  *info={28064,100000,6388,13356,100,7,107};
}
bool heap_caps_check_integrity_all(bool printErrors) { assert(!printErrors); ++checks; return intact; }
int ets_printf(const char* format,...) {
  ++logs; va_list args;va_start(args,format);
  int count=std::vsnprintf(captured,sizeof(captured),format,args);va_end(args);return count;
}
#endif
int main() {
  const unsigned before=allocations;
  CM_HTTP_HEAP_TRACE("/appinfo","cors-methods");
  assert(allocations==before);
#if CM_ENABLE_VERBOSE_LOGGING
  assert(queries==1 && checks==1 && logs==1);
  assert(std::strstr(captured,"route=/appinfo stage=cors-methods"));
  assert(std::strstr(captured,"free=28064 largest=6388 min=13356"));
  assert(std::strstr(captured,"blocks=7 integrity=ok"));
  intact=false;CM_HTTP_HEAP_TRACE("/appinfo","start");
  assert(std::strstr(captured,"integrity=CORRUPT"));
  assert(allocations==before);
#endif
  std::puts("HTTP heap diagnostics passed: compile guard, snapshot fields, integrity and no C++ allocations.");
}
