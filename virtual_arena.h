#ifndef _VIRTUAL_ARENA_HEADER
#define _VIRTUAL_ARENA_HEADER
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
typedef struct VirtualArena {
    uint8_t*        __memory;    // Base pointer to reserved memory
    uintptr_t       __position;  // Current allocation position
    uintptr_t       __commited_size;
    uintptr_t       __total_size;  // Size
    pthread_mutex_t __arena_mutex;
    int             __auto_align;
    int             __alignment;
    // VirtualArena*    __parent;
} VirtualArena;

int Init_VirtualArena(VirtualArena* arena, int arena_size, int auto_align) {
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
  arena->__commited_size = _getPageSize();
#ifdef _WIN32
  // arena->__memory = (uint8_t*)VirtualAlloc(nullptr, arena_size, MEM_RESERVE, PAGE_NOACCESS);
  arena->__memory = (uint8_t*)VirtualAlloc(NULL, arena_size,
                                           MEM_RESERVE,  // Combined flags
                                           PAGE_READWRITE);
  if (!arena->__memory) {
    return -1;
  }
  void* committed = VirtualAlloc(arena->__memory, arena->__commited_size, MEM_COMMIT, PAGE_READWRITE);
  if (!committed) {
    VirtualFree(arena->__memory, 0, MEM_RELEASE);
    return -1;
  }
#else
  // On Unix-like systems, we use mmap with PROT_NONE
  arena->__memory = mmap(NULL, arena_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (arena->__memory == MAP_FAILED) {
    return -1;
  }
  if (madvise(arena->__memory, arena->__committed_size, MADV_WILLNEED) != 0) {
    munmap(arena->memory, arena_size);
    return -1;
  }
#endif
  return 0;
}
// Here
int Destroy_VirtualArena(VirtualArena* arena) {
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
  arena->__memory        = NULL;
  arena->__total_size    = 0;
  arena->__commited_size = 0;
  arena->__position      = 0;
  arena->__auto_align    = 0;
  arena->__alignment     = 0;
  pthread_mutex_unlock(&arena->__arena_mutex);
  pthread_mutex_destroy(&arena->__arena_mutex);
  return 0;
}

int SetAutoAlign2Pow_VirtualArena(VirtualArena* arena, int alignment) {
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

uintptr_t GetPos_VirtualArena(VirtualArena* arena) {
  if (arena == NULL) {
    return NULL;
  }
  return arena->__position;
}

int PushAligner_VirtualArena(VirtualArena* arena, int alignment) {
  if (arena == NULL) {
    return -1;
  }
  pthread_mutex_lock(&arena->__arena_mutex);
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    pthread_mutex_unlock(&arena->__arena_mutex);
    return -1;
  }
  arena->__position = align_2pow(arena->__position + (uintptr_t)arena->__memory, alignment) - (uintptr_t)arena->__memory;
  pthread_mutex_unlock(&arena->__arena_mutex);
  return 0;
}
uint8_t* PushNoZero_VirtualArena(VirtualArena* arena, int bytes) {
  if (arena == NULL) {
    return NULL;
  }
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
uint8_t* Push_VirtualArena(VirtualArena* arena, int bytes) {
  if (arena == NULL) {
    return NULL;
  }
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

int Pop_VirtualArena(VirtualArena* arena, uintptr_t bytes) {
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
int PopTo_VirtualArena(VirtualArena* arena, uintptr_t position) {
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
int PopToAdress_VirtualArena(VirtualArena* arena, uint8_t* address) {
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
int Clear_VirtualArena(VirtualArena* arena) {
  if (arena == NULL) {
    return -1;
  }
  pthread_mutex_lock(&arena->__arena_mutex);
  arena->__position = 0;
  pthread_mutex_unlock(&arena->__arena_mutex);
  return 0;
}

// Essentially, the scratch space is another arena of the same type rooted at the top pointer. Only works for static I guess.
int InitScratch_VirtualArena(VirtualArena* scratch_space, VirtualArena* arena, int arena_size, int auto_align) {
  if (scratch_space == NULL || scratch_space == NULL) {
    return -1;
  }
  pthread_mutex_lock(&arena->__arena_mutex);
  uint8_t* mem = PushNoZero_VirtualArena(arena, arena_size);
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
  return 0;
}
int DestroyScratch_VirtualArena(VirtualArena* scratch_space, VirtualArena* parent_arena) {
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
int MergeScratch_VirtualArena(VirtualArena* scratch_space, VirtualArena* parent_arena) {
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