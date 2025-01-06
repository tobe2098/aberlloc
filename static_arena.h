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
  if (arena == NULL) {
    return -1;
  }
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
  if (arena == NULL) {
    return -1;
  }
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
  if (arena == NULL) {
    return -1;
  }
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
  if (arena == NULL) {
    return NULL;
  }
  return arena->__position;
}

int PushAligner_StaticArena(StaticArena* arena, int alignment) {
  if (arena == NULL) {
    return -1;
  }
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    return -1;
  }
  pthread_mutex_lock(&arena->__arena_mutex);
  arena->__position = align_2pow(arena->__position + (uintptr_t)arena->__memory, alignment) - (uintptr_t)arena->__memory;
  pthread_mutex_unlock(&arena->__arena_mutex);
  return 0;
}
uint8_t* PushNoZero_StaticArena(StaticArena* arena, int bytes) {
  if (arena == NULL) {
    return NULL;
  }
  pthread_mutex_lock(&arena->__arena_mutex);
  if (arena->__auto_align) {
    arena->__position = align_2pow(arena->__position, arena->__alignment);
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
  if (arena == NULL) {
    return NULL;
  }
  pthread_mutex_lock(&arena->__arena_mutex);
  if (arena->__auto_align) {
    arena->__position = align_2pow(arena->__position, arena->__alignment);
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

int Pop_StaticArena(StaticArena* arena, uintptr_t bytes) {
  // Be careful, if auto align is on, the aligner allocated bytes are unseen to you. You should use pop to position or address if autoalign
  // is on.
  if (arena == NULL) {
    return -1;
  }
  pthread_mutex_lock(&arena->__arena_mutex);
  if (arena->__position < bytes) {
    bytes = arena->__position;
  }
  arena->__position -= bytes;
  pthread_mutex_unlock(&arena->__arena_mutex);
  return 0;
}
int PopTo_StaticArena(StaticArena* arena, uintptr_t position) {
  if (arena == NULL) {
    return -1;
  }
  pthread_mutex_lock(&arena->__arena_mutex);
  if (position < arena->__position) {
    arena->__position = position;
  }
  pthread_mutex_unlock(&arena->__arena_mutex);
  return 0;
}
int PopToAdress_StaticArena(StaticArena* arena, uint8_t* address) {
  if (arena == NULL) {
    return -1;
  }
  pthread_mutex_lock(&arena->__arena_mutex);
  uintptr_t final_position = address - arena->__memory;
  if ((uintptr_t)(arena->__memory) < (uintptr_t)address) {
    arena->__position = final_position;
  }
  pthread_mutex_unlock(&arena->__arena_mutex);
  return 0;
}
int Clear_StaticArena(StaticArena* arena) {
  if (arena == NULL) {
    return -1;
  }
  pthread_mutex_lock(&arena->__arena_mutex);
  arena->__position = 0;
  pthread_mutex_unlock(&arena->__arena_mutex);
  return 0;
}

// Essentially, the scratch space is another arena of the same type rooted at the top pointer. Only works for static I guess.
int InitScratch_StaticArena(StaticArena* scratch_space, StaticArena* arena, int arena_size, int auto_align) {
  if (scratch_space == NULL || scratch_space == NULL) {
    return -1;
  }
  pthread_mutex_lock(&arena->__arena_mutex);
  uint8_t* mem = PushNoZero_StaticArena(arena, arena_size);
  if (mem == NULL) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return -1;
  }

  scratch_space->__memory = mem;
  if (scratch_space->__memory == NULL) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return -1;
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
  pthread_mutex_unlock(&arena->__arena_mutex);
  return scratch_space;
}
int DestroyScratch_StaticArena(StaticArena* scratch_space, StaticArena* parent_arena) {
  // Destructor must run under locked mutex of parent to make sure of correct behaviour.
  pthread_mutex_lock(&scratch_space->__arena_mutex);
  pthread_mutex_lock(&(parent_arena->__arena_mutex));
  // Check for position overflow in the memory pop.
  if (parent_arena->__position < scratch_space->__total_size) {
    parent_arena->__position = scratch_space->__total_size;
  }
  // Null properties and pop memory
  parent_arena->__position -= scratch_space->__total_size;
  scratch_space->__memory     = NULL;
  scratch_space->__total_size = 0;
  scratch_space->__position   = 0;
  scratch_space->__auto_align = 0;
  scratch_space->__alignment  = 0;
  pthread_mutex_unlock(&scratch_space->__arena_mutex);
  pthread_mutex_destroy(&scratch_space->__arena_mutex);
  pthread_mutex_unlock(&(parent_arena->__arena_mutex));
  return 0;
}
int MergeScratch_StaticArena(StaticArena* scratch_space, StaticArena* parent_arena) {
  // Merger must run under locked mutex of parent to make sure of correct behaviour.
  pthread_mutex_lock(&scratch_space->__arena_mutex);
  pthread_mutex_lock(&(parent_arena->__arena_mutex));
  // Set the new position to conserve the memory from the scratch space and null properties
  // No need to do bounds check as the memory addresses must be properly ordered, and the position too.
  parent_arena->__position    = ((uintptr_t)scratch_space->__memory - (uintptr_t)parent_arena->__memory) + scratch_space->__position;
  scratch_space->__memory     = NULL;
  scratch_space->__total_size = 0;
  scratch_space->__position   = 0;
  scratch_space->__auto_align = 0;
  scratch_space->__alignment  = 0;
  pthread_mutex_unlock(&scratch_space->__arena_mutex);
  pthread_mutex_destroy(&scratch_space->__arena_mutex);

  pthread_mutex_unlock(&(parent_arena->__arena_mutex));
  return 0;
}

#endif