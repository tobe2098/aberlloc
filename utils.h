#ifndef _UTILS_ABERLLOC_HEADER
#define _UTILS_ABERLLOC_HEADER
#include <unistd.h>
#include <windows.h>
typedef int bool;
#define TRUE      1
#define FALSE     0

#define WORD_SIZE sizeof(void*)

uintptr_t align_address(uintptr_t addr, size_t align) {
  if (align == 0) {
    return addr;
  }
  return addr + (align - (addr % align)) % align;
}

static inline uintptr_t align_2pow(uintptr_t n, size_t align) {
  return (n + align - 1) & ~(align - 1);
}

static size_t _getPageSize(void) {
#ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
#else
  return sysconf(_SC_PAGESIZE);
#endif
}

#endif