#ifndef _UTILS_ABERLLOC_HEADER
#define _UTILS_ABERLLOC_HEADER
#include <unistd.h>
#include <windows.h>
// typedef unsigned long long size_t;
typedef int bool;
#define TRUE                 1
#define FALSE                0

#define WORD_SIZE            sizeof(void*)

#define SMALL_SCRATCH_SPACE  1024 * 512       // 512 kB
#define MEDIUM_SCRATCH_SPACE 1024 * 1024 * 1  // 1 MB
#define LARGE_SCRATCH_SPACE  1024 * 1024 * 4  // 4 MB

#define SMALL_SIZE_ARENA     1024 * 1024 * 64    // 64 MB
#define MEDIUM_SIZE_ARENA    1024 * 1024 * 256   // 256 MB
#define LARGE_SIZE_ARENA     1024 * 1024 * 1024  // 1 GB

uintptr_t align_address(uintptr_t addr, uintptr_t align) {
  if (align == 0) {
    return addr;
  }
  return addr + (align - (addr % align)) % align;
}

static inline uintptr_t align_2pow(uintptr_t n, uintptr_t align) {
  return (n + align - 1) & ~(align - 1);
}

static size_t PAGE_SIZE = 0;

size_t _getPageSize(void) {
  if (!PAGE_SIZE) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    PAGE_SIZE = si.dwPageSize;
#else
    PAGE_SIZE = sysconf(_SC_PAGESIZE);
#endif
  }
  return PAGE_SIZE;
}

#endif