#ifndef _VIRTUAL_DYNAMIC_ARENA_HEADER
#define _VIRTUAL_DYNAMIC_ARENA_HEADER
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
typedef struct SimpleArena {
    uint8_t*        __memory;    // Base pointer to reserved memory
    size_t          __position;  // Current allocation position
    size_t          __commited_size;
    size_t          __total_size;  // Size
    pthread_mutex_t __arena_mutex;
    int             __auto_align;
} SimpleArena;

int Alloc_SimpleArena(SimpleArena* arena, int arena_size) {
  pthread_mutex_init(&arena->__arena_mutex, NULL);
  arena->__total_size = arena_size;
  arena->__position   = 0;
#ifdef _WIN32
  arena->__memory = (uint8_t*)VirtualAlloc(nullptr, arena_size, MEM_RESERVE, PAGE_NOACCESS);
  if (!arena->__memory) {
    return -1;
  }
#else
  // On Unix-like systems, we use mmap with PROT_NONE
  arena->__memory = mmap(nullptr, arena_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    return -1;
  }
#endif
  return 0;
}
int Release_SimpleArena(SimpleArena* arena) { }

int SetAutoAlign2Pow_SimpleArena(SimpleArena* arena, int alignment) {
  pthread_mutex_lock(&arena->__arena_mutex);
  if (__builtin_popcount(alignment) != 1) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return -1;
  }
  arena->__auto_align = alignment;
  pthread_mutex_unlock(&arena->__arena_mutex);
  return 0;
}
int GetPos_SimpleArena(SimpleArena* arena) { }

int PushAligner_SimpleArena(SimpleArena* arena, int alignemnt) { }
int PushNoZero_SimpleArena(SimpleArena* arena, int bytes) { }
int Push_SimpleArena(SimpleArena* arena, int bytes) { }

int PopTo_SimpleArena(SimpleArena* arena, int position) { }
int Pop_SimpleArena(SimpleArena* arena, int bytes) { }
int Clear_SimpleArena(SimpleArena* arena) { }

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