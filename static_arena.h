#ifndef _STATIC_ARENA_HEADER
#define _STATIC_ARENA_HEADER
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include "./utils.h"
#ifdef _WIN32
#ifdef __GNUC__
#include <windows.h>
// Compilation using msys2 env or similar
#else
#error "You need to compile with gcc."
#endif
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
// Fixed size arena, only manages power of two alignments based on word size (if not power of 2, error)
// Single-threaded
typedef struct StaticArena {
    uint8_t* __memory;      // Base pointer to reserved memory
    size_t   __position;    // Current allocation position
    size_t   __total_size;  // Size
    // pthread_mutex_t __arena_mutex;
    int __auto_align;
} StaticArena;

int Init_StaticArena(StaticArena* arena, int arena_size, int auto_align) {
  // pthread_mutex_init(&arena->__arena_mutex, NULL);
  arena->__total_size = arena_size;
  arena->__position   = 0;
  int word_size       = WORD_SIZE;
  if (auto_align > word_size && __builtin_popcount(auto_align) == 1) {
    arena->__auto_align = auto_align;
  } else {
    arena->__auto_align = word_size;
  }
#ifdef _WIN32
  // arena->__memory = (uint8_t*)VirtualAlloc(nullptr, arena_size, MEM_RESERVE, PAGE_NOACCESS);
  arena->__memory = (uint8_t*)VirtualAlloc(NULL, arena_size,
                                           MEM_RESERVE | MEM_COMMIT,  // Combined flags
                                           PAGE_READWRITE);
  if (!arena->__memory) {
    return -1;
  }
#else
  // On Unix-like systems, we use mmap with PROT_NONE
  arena->__memory = mmap(NULL, arena_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    return -1;
  }
#endif
  return 0;
}
int Destroy_StaticArena(StaticArena* arena) {
  // pthread_mutex_lock(&arena->__arena_mutex);
#ifdef _WIN32
  if (VirtualFree(arena->__memory, 0, MEM_RELEASE) != 0) {
    return -1;
  }
#else
  if (munmap(arena->__memory, arena->__total_size) != 0) {
    return -1;
  }
#endif
  arena->__memory     = NULL;
  arena->__total_size = 0;
  arena->__position   = 0;
  arena->__auto_align = 0;
  // pthread_mutex_unlock(&arena->__arena_mutex);
  // pthread_mutex_destroy(&arena->__arena_mutex);
  return 0;
}

int SetAutoAlign2Pow_StaticArena(StaticArena* arena, int alignment) {
  // pthread_mutex_lock(&arena->__arena_mutex);
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    // pthread_mutex_unlock(&arena->__arena_mutex);
    return -1;
  }
  arena->__auto_align = alignment;
  // pthread_mutex_unlock(&arena->__arena_mutex);
  return 0;
}
int GetPos_StaticArena(StaticArena* arena) { }

int PushAligner_StaticArena(StaticArena* arena, int alignemnt) { }
int PushNoZero_StaticArena(StaticArena* arena, int bytes) { }
int Push_StaticArena(StaticArena* arena, int bytes) { }

int PopTo_StaticArena(StaticArena* arena, int position) { }
int Pop_StaticArena(StaticArena* arena, int bytes) { }
int Clear_StaticArena(StaticArena* arena) { }

// Scratch space with ret value option. The idea is that the scratch space handles the deallocation, you allocate with the arena pointer
typedef struct SA_Scratch {
    uint8_t* __memory;      // Base pointer to reserved memory
    size_t   __position;    // Current allocation position
    size_t   __total_size;  // Size
    // pthread_mutex_t __arena_mutex;
    int __auto_align;
} SA_Scratch;

// ArenaAlloc: Creates new arena with reserved virtual memory
// ArenaRelease: Releases all arena memory
// ArenaSetAutoAlign: Controls automatic alignment behavior
// ArenaPos: Returns current allocation position
// ArenaPushNoZero: Allocates uninitialized memory
// ArenaPushAligner: Adds alignment padding
// ArenaPush: Allocates zero-initialized memory
// ArenaPopTo: Returns to specific position
// ArenaPop: Removes specified amount from top
// ArenaClear: Resets arena to empty state

#endif