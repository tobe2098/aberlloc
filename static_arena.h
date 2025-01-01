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
    uint8_t*        __memory;      // Base pointer to reserved memory
    uintptr_t       __position;    // Current allocation position
    uintptr_t       __total_size;  // Size
    pthread_mutex_t __arena_mutex;
    int             __auto_align;
    int             __alignment;
    // StaticArena*    __parent;
} StaticArena;

int Init_StaticArena(StaticArena* arena, int arena_size, int auto_align) {
  pthread_mutex_init(&arena->__arena_mutex, NULL);
  arena->__total_size = arena_size;
  arena->__position   = 0;
  // arena->__parent     = NULL;
  int word_size = WORD_SIZE;
  if (auto_align > word_size && __builtin_popcount(auto_align) == 1) {
    arena->__auto_align = TRUE;
    arena->__alignment  = auto_align;
  } else {
    arena->__auto_align = FALSE;
    arena->__alignment  = word_size;
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
  pthread_mutex_lock(&arena->__arena_mutex);
#ifdef _WIN32
  if (VirtualFree(arena->__memory, 0, MEM_RELEASE) != 0) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return -1;
  }
#else
  if (munmap(arena->__memory, arena->__total_size) != 0) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return -1;
  }
#endif
  arena->__memory     = NULL;
  arena->__total_size = 0;
  arena->__position   = 0;
  arena->__auto_align = 0;
  pthread_mutex_unlock(&arena->__arena_mutex);
  pthread_mutex_destroy(&arena->__arena_mutex);
  return 0;
}

int SetAutoAlign2Pow_StaticArena(StaticArena* arena, int alignment) {
  pthread_mutex_lock(&arena->__arena_mutex);
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return -1;
  }
  arena->__auto_align = TRUE;
  arena->__alignment  = alignment;
  pthread_mutex_unlock(&arena->__arena_mutex);
  return 0;
}

uintptr_t GetPos_StaticArena(StaticArena* arena) {
  return arena->__position;
}

int PushAligner_StaticArena(StaticArena* arena, int alignment) {
  pthread_mutex_lock(&arena->__arena_mutex);
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return -1;
  }
  arena->__position = align_2pow(arena->__position + (uintptr_t)arena->__memory, alignment) - (uintptr_t)arena->__memory;
  pthread_mutex_unlock(&arena->__arena_mutex);
  return 0;
}
uint8_t* PushNoZero_StaticArena(StaticArena* arena, int bytes) {
  pthread_mutex_lock(&arena->__arena_mutex);
  if (arena->__auto_align) {
    arena->__position = align_2pow(arena->__position + (uintptr_t)arena->__memory, arena->__alignment) - (uintptr_t)arena->__memory;
  }
  if (arena->__position + bytes > arena->__total_size) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return NULL;
  }
  uint8_t* ptr = arena->__memory + arena->__position;
  arena->__position += bytes;
  pthread_mutex_unlock(&arena->__arena_mutex);
  return ptr;
}
uint8_t* Push_StaticArena(StaticArena* arena, int bytes) {
  pthread_mutex_lock(&arena->__arena_mutex);
  if (arena->__auto_align) {
    arena->__position = align_2pow(arena->__position + (uintptr_t)arena->__memory, arena->__alignment) - (uintptr_t)arena->__memory;
  }
  if (arena->__position + bytes > arena->__total_size) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return NULL;
  }
  uint8_t* ptr = arena->__memory + arena->__position;
  arena->__position += bytes;
  pthread_mutex_unlock(&arena->__arena_mutex);
  memset(ptr, 0, bytes);
  return ptr;
}

void Pop_StaticArena(StaticArena* arena, uintptr_t bytes) {
  pthread_mutex_lock(&arena->__arena_mutex);
  if (arena->__position < bytes) {
    bytes = arena->__position;
  }
  arena->__position -= bytes;
  pthread_mutex_unlock(&arena->__arena_mutex);
}
void PopTo_StaticArena(StaticArena* arena, uintptr_t position) {
  pthread_mutex_lock(&arena->__arena_mutex);
  if (position < arena->__position) {
    arena->__position = position;
  }
  pthread_mutex_unlock(&arena->__arena_mutex);
}
int PopToAdress_StaticArena(StaticArena* arena, uint8_t* address) {
  pthread_mutex_lock(&arena->__arena_mutex);
  uintptr_t final_position = address - arena->__memory;
  if ((uintptr_t)(arena->__memory) < (uintptr_t)address) {
    arena->__position = final_position;
  }
  pthread_mutex_unlock(&arena->__arena_mutex);
}
int Clear_StaticArena(StaticArena* arena) {
  pthread_mutex_lock(&arena->__arena_mutex);
  arena->__position = 0;
  pthread_mutex_unlock(&arena->__arena_mutex);
}

// Essentially, the scratch space is another arena of the same type rooted at the top pointer. Only works for static I guess.
StaticArena* AllocScratch_StaticArena(StaticArena* arena, int arena_size, int auto_align) {
  // To ensure correct behaviour, the scratch space locks the parent arena down.
  pthread_mutex_lock(&arena->__arena_mutex);
  uint8_t* mem = PushNoZero_StaticArena(arena, arena_size);
  if (mem == NULL) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return NULL;
  }
  StaticArena* scratch_space = PushNoZero_StaticArena(arena, sizeof(StaticArena));
  if (scratch_space == NULL) {
    Pop_StaticArena(arena, arena_size);
    pthread_mutex_unlock(&arena->__arena_mutex);
    return NULL;
  }
  scratch_space->__memory = mem;
  if (scratch_space->__memory == NULL) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return NULL;
  }
  scratch_space->__total_size = arena_size;
  scratch_space->__position   = 0;
  int word_size               = WORD_SIZE;
  if (auto_align > word_size && __builtin_popcount(auto_align) == 1) {
    scratch_space->__auto_align = TRUE;
    scratch_space->__alignment  = auto_align;
  } else {
    scratch_space->__auto_align = FALSE;
    scratch_space->__alignment  = word_size;
  }
  pthread_mutex_init(&scratch_space->__arena_mutex, NULL);
  return scratch_space;
}
int FreeScratch_StaticArena(StaticArena* scratch_space) {
  pthread_mutex_lock(&scratch_space->__arena_mutex);
  // Destructor must run under locked mutex of parent to make sure of correct behaviour.
  if (scratch_space->__parent->__position < scratch_space->__total_size + sizeof(StaticArena)) {
    scratch_space->__parent->__position = sizeof(StaticArena) + scratch_space->__total_size;
  }
  // Null properties
  uintptr_t size              = scratch_space->__total_size;
  scratch_space->__memory     = NULL;
  scratch_space->__total_size = 0;
  scratch_space->__position   = 0;
  scratch_space->__auto_align = 0;
  pthread_mutex_unlock(&scratch_space->__arena_mutex);
  pthread_mutex_destroy(&scratch_space->__arena_mutex);
  // Pop memory
  scratch_space->__parent->__position -= (size + sizeof(StaticArena));
  pthread_mutex_unlock(&(scratch_space->__parent->__arena_mutex));
  scratch_space->__parent = NULL;
  return 0;
}
int MergeScratch_StaticArena(StaticArena* scratch_space) {
  pthread_mutex_lock(&scratch_space->__arena_mutex);
  // Destructor must run under locked mutex of parent to make sure of correct behaviour.
  if (scratch_space->__parent->__position < scratch_space->__total_size + sizeof(StaticArena)) {
    scratch_space->__parent->__position = sizeof(StaticArena) + scratch_space->__total_size;
  }
  // Null properties
  scratch_space->__memory     = NULL;
  scratch_space->__total_size = 0;
  scratch_space->__position   = 0;
  scratch_space->__auto_align = 0;

  pthread_mutex_unlock(&scratch_space->__arena_mutex);
  pthread_mutex_destroy(&scratch_space->__arena_mutex);
  // Pop memory
  scratch_space->__parent->__position -= sizeof(StaticArena);

  pthread_mutex_unlock(&(scratch_space->__parent->__arena_mutex));
  scratch_space->__parent = NULL;
  return 0;
}

#endif