#ifndef _UTILS_ABERLLOC_HEADER
#define _UTILS_ABERLLOC_HEADER
#include <unistd.h>
#include <windows.h>
#include "cache.h"
// typedef unsigned long long size_t;
typedef int bool;
#define TRUE                   1
#define FALSE                  0

#define ERROR_OS_MEMORY        -1
#define ERROR_INVALID_PARAMS   -2

#define WORD_SIZE              sizeof(void*)
#define CROSS_THREAD_ALIGNMENT CACHE_LINE_SIZE

#define SMALL_SCRATCH_SPACE    1024 * 512       // 512 kB
#define MEDIUM_SCRATCH_SPACE   1024 * 1024 * 1  // 1 MB
#define LARGE_SCRATCH_SPACE    1024 * 1024 * 4  // 4 MB

#define SMALL_SIZE_ARENA       1024 * 1024 * 64    // 64 MB
#define MEDIUM_SIZE_ARENA      1024 * 1024 * 256   // 256 MB
#define LARGE_SIZE_ARENA       1024 * 1024 * 1024  // 1 GB

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

inline uintptr_t extendPolicy(uintptr_t size) {
  return size * 4;
}
inline uintptr_t reducePolicy(uintptr_t size) {
  return size / 2;
}
inline uint8_t* _os_new_virtual_mapping(size_t size) {
  // We want to return ptr on success, NULL on failure
#ifdef _WIN32
  return ((uint8_t*)VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE));
#else
  uint8_t* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return (ptr != MAP_FAILED) ? ptr : NULL;
#endif
}

inline int _os_commit(void* base_ptr, size_t size) {
#ifdef _WIN32
  return (VirtualAlloc(base_ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL) ? 0 : -1;
#else
  // On Unix-like systems, it is more of a suggestion
  return (madvise(base_ptr, size, MADV_WILLNEED) != 0) ? 0 : -1;
#endif
}
#ifndef DEBUG
inline void _os_free(void* base_ptr, size_t size) {
#ifdef _WIN32
  VirtualFree(base_ptr, 0, MEM_RELEASE);
#else
  munmap(arena->memory, arena_size);
#endif
}
#else
inline int _os_free(void* base_ptr, size_t size) {
#ifdef _WIN32
  return (VirtualFree(base_ptr, 0, MEM_RELEASE) != 0);
#else
  return munmap(arena->memory, arena_size);
#endif
}
#endif
#endif