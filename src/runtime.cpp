// runtime.cpp - the few C library functions the compiler may call on its own.
// With bulk-memory enabled, these become single WebAssembly memory.copy / memory.fill ops.
#include "core.h"

extern "C" {
void* memcpy(void* dst, const void* src, usize n) { return __builtin_memcpy(dst, src, n); }
void* memset(void* dst, int v, usize n) { return __builtin_memset(dst, v, n); }
void* memmove(void* dst, const void* src, usize n) { return __builtin_memmove(dst, src, n); }
}
