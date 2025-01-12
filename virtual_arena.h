#ifndef _VIRTUAL_ARENA_HEADER
#define _VIRTUAL_ARENA_HEADER
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include "./static_arena.h"
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
    uint8_t*  __memory;    // Base pointer to reserved memory
    uintptr_t __position;  // Current allocation position
    uintptr_t __committed_size;
    uintptr_t __total_size;  // Size
    // pthread_mutex_t __arena_mutex;
    int __auto_align;
    int __alignment;
    // VirtualArena*    __parent;
} VirtualArena;

int Init_VirtualArena(VirtualArena* arena, int arena_size, int auto_align) {
  if (arena == NULL) {
    return -1;
  }
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
  arena->__committed_size = _getPageSize();
  arena->__memory         = _os_new_virtual_mapping(arena->__total_size);
  if (arena->__memory == NULL) {
    return -1;
  }
  if (_os_commit(arena->__memory, arena->__committed_size) == -1) {
    _os_free(arena->__memory, arena->__total_size);
    return -1;
  }
  return 0;
}
// Here
int Destroy_VirtualArena(VirtualArena* arena) {
  if (arena == NULL) {
    return -1;
  }
#ifdef DEBUG
  if (_os_free(arena->__memory, arena->__total_size) == -1) {
    return -1;
  }
#else
  _os_free(arena->__memory, arena->__total_size);
#endif

  arena->__memory         = NULL;
  arena->__total_size     = 0;
  arena->__committed_size = 0;
  arena->__position       = 0;
  arena->__auto_align     = 0;
  arena->__alignment      = 0;
  return 0;
}

int SetAutoAlign2Pow_VirtualArena(VirtualArena* arena, int alignment) {
  if (arena == NULL) {
    return -1;
  }
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    return -1;
  }
  arena->__auto_align = TRUE;
  arena->__alignment  = alignment;
  return 0;
}

int ReMap_VirtualArena(VirtualArena* arena, int total_size) {
  if (total_size < arena->__committed_size) {
    // Need to ensure there is enough space at destination of memcopy
    return -1;
  }
  uint8_t* new_memory = _os_new_virtual_mapping(total_size);
  if (new_memory == NULL) {
    return -1;
  }
  memcpy(new_memory, arena->__memory, arena->__position);
  // We cannot tolerate failure after this, as we have two blocks of memory to manage. It has to be freed
#ifdef _WIN32
  VirtualFree(arena->__memory, 0, MEM_RELEASE);
#else
  munmap(arena->memory, arena->__total_size);
#endif
  arena->__memory = new_memory;
  return 0;
}

int ExtendCommit_VirtualArena(VirtualArena* arena, int total_commited_size) {
  if (!arena || !total_commited_size || total_commited_size < arena->__committed_size) {
    return -1;
  }
#ifdef AUTO_REMAP
  if (total_commited_size > arena->__total_size) {
    if (!ReMap_VirtualArena(arena, extendPolicy(arena->__total_size))) {
      return -1;
    }
  }
#else
  if (total_commited_size > arena->__total_size) {
    return -1;
  }
#endif
  // We only need to extend the memory commitment under the total size.
#ifdef _WIN32
  void* committed = VirtualAlloc(arena->__memory, total_commited_size, MEM_COMMIT, PAGE_READWRITE);
  if (!committed) {
    // If the call fails, is there something to take care of?
    return -1;
  }
#else
  // On Unix-like systems, it is more of a suggestion
  arena->__memory = mmap(NULL, arena_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (arena->__memory == MAP_FAILED) {
    return -1;
  }
  void* committed = madvise(arena->__memory, total_commited_size, MADV_WILLNEED);
#endif
  arena->__committed_size = total_commited_size;
}
int ReduceCommit_VirtualArena(VirtualArena* arena, int total_commited_size) {
#ifdef DEBUG
  if (!arena || !total_commited_size || total_commited_size > arena->__commited_size) {
    return -1;
  }
#endif
#ifdef _WIN32
  void* committed = VirtualAlloc(arena->__memory, arena->__committed_size, MEM_COMMIT, PAGE_READWRITE);
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
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    return -1;
  }
  arena->__position = align_2pow(arena->__position, alignment);
  // arena->__position = align_2pow(arena->__position + (uintptr_t)arena->__memory, alignment) - (uintptr_t)arena->__memory;
  return 0;
}
uint8_t* PushNoZero_VirtualArena(VirtualArena* arena, int bytes) {
  if (arena == NULL) {
    return NULL;
  }
  if (arena->__auto_align) {
    arena->__position = align_2pow(arena->__position, arena->__alignment);
  }
  if (arena->__position + bytes > arena->__total_size) {
    return NULL;
  }
  uint8_t* ptr = arena->__memory + arena->__position;
  arena->__position += bytes;
  return ptr;
}
uint8_t* Push_VirtualArena(VirtualArena* arena, int bytes) {
  if (arena == NULL) {
    return NULL;
  }
  if (arena->__auto_align) {
    arena->__position = align_2pow(arena->__position, arena->__alignment);
  }
  if (arena->__position + bytes > arena->__total_size) {
    return NULL;
  }
  uint8_t* ptr = arena->__memory + arena->__position;
  arena->__position += bytes;
  memset(ptr, 0, bytes);
  return ptr;
}

int Pop_VirtualArena(VirtualArena* arena, uintptr_t bytes) {
  // Be careful, if auto align is on, the aligner allocated bytes are unseen to you. You should use pop to position or address if autoalign
  // is on.
  if (arena == NULL) {
    return -1;
  }
  if (arena->__position < bytes) {
    bytes = arena->__position;
  }
  arena->__position -= bytes;
  return 0;
}
int PopTo_VirtualArena(VirtualArena* arena, uintptr_t position) {
  if (arena == NULL) {
    return -1;
  }
  if (position < arena->__position) {
    arena->__position = position;
  }
  return 0;
}
int PopToAdress_VirtualArena(VirtualArena* arena, uint8_t* address) {
  if (arena == NULL) {
    return -1;
  }
  uintptr_t final_position = address - arena->__memory;
  if ((uintptr_t)(arena->__memory) < (uintptr_t)address) {
    arena->__position = final_position;
  }
  return 0;
}
int Clear_VirtualArena(VirtualArena* arena) {
  if (arena == NULL) {
    return -1;
  }
  arena->__position = 0;
  return 0;
}

// Essentially, the scratch space is another arena of the same type rooted at the top pointer. Only works for static I guess.
int InitScratch_VirtualArena(StaticArena* scratch_space, VirtualArena* arena, int arena_size, int auto_align) {
  if (scratch_space == NULL || scratch_space == NULL) {
    return -1;
  }
  uint8_t* mem = PushNoZero_VirtualArena(arena, arena_size);
  if (mem == NULL) {
    return -1;
  }

  scratch_space->__memory = mem;
  if (scratch_space->__memory == NULL) {
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
  return 0;
}
int DestroyScratch_VirtualArena(StaticArena* scratch_space, VirtualArena* parent_arena) {
  // Destructor must run under locked mutex of parent to make sure of correct behaviour.
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
  return 0;
}
int MergeScratch_VirtualArena(StaticArena* scratch_space, VirtualArena* parent_arena) {
  // Merger must run under locked mutex of parent to make sure of correct behaviour.
  // Set the new position to conserve the memory from the scratch space and null properties
  // No need to do bounds check as the memory addresses must be properly ordered, and the position too.
  parent_arena->__position    = ((uintptr_t)scratch_space->__memory - (uintptr_t)parent_arena->__memory) + scratch_space->__position;
  scratch_space->__memory     = NULL;
  scratch_space->__total_size = 0;
  scratch_space->__position   = 0;
  scratch_space->__auto_align = 0;
  scratch_space->__alignment  = 0;
  return 0;
}

#endif