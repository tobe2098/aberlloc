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
    uint8_t*  memory_;      // Base pointer to reserved memory
    uintptr_t position_;    // Current allocation position
    uintptr_t total_size_;  // Size
    // pthread_mutex_t __arena_mutex;
    int auto_align_;
    int alignment_;
    // StaticArena*    __parent;
} StaticArena;

int Init_StaticArena(StaticArena* arena, int arena_size, int auto_align) {
#ifdef DEBUG
  if (arena == NULL) {
    return -1;
  }
#endif
  arena->total_size_ = arena_size;
  arena->position_   = 0;
  // arena->__parent     = NULL;
  int word_size = WORD_SIZE;
  if (auto_align > word_size && __builtin_popcount(auto_align) == 1) {
    arena->auto_align_ = TRUE;
    arena->alignment_  = auto_align;
  } else {
    arena->auto_align_ = FALSE;
    arena->alignment_  = word_size;
  }
  arena->memory_ = os_new_virtual_mapping_(arena_size);
  if (arena->memory_ == NULL) {
    return -1;
  }
  return 0;
}
int Destroy_StaticArena(StaticArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return -1;
  }
#endif
  if (os_free_(arena->memory_, arena->total_size_) == -1) {
    return -1;
  }
  arena->memory_     = NULL;
  arena->total_size_ = 0;
  arena->position_   = 0;
  arena->auto_align_ = 0;
  return 0;
}

int SetAutoAlign2Pow_StaticArena(StaticArena* arena, int alignment) {
#ifdef DEBUG
  if (arena == NULL) {
    return -1;
  }
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    return -1;
  }
#endif
  arena->auto_align_ = TRUE;
  arena->alignment_  = alignment;
  return 0;
}

uintptr_t GetPos_StaticArena(StaticArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  return arena->position_;
}

int PushAligner_StaticArena(StaticArena* arena, int alignment) {
#ifdef DEBUG
  if (arena == NULL) {
    return -1;
  }
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    return -1;
  }
#endif
  arena->position_ = align_2pow(arena->position_ + (uintptr_t)arena->memory_, alignment) - (uintptr_t)arena->memory_;
  return 0;
}

int PushAlignerCacheLine_StaticArena(StaticArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return -1;
  }
#endif
  arena->position_ = align_2pow(arena->position_ + (uintptr_t)arena->memory_, CACHE_LINE_SIZE) - (uintptr_t)arena->memory_;
  return 0;
}

uint8_t* PushNoZero_StaticArena(StaticArena* arena, int bytes) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  if (arena->auto_align_) {
    PushAligner_StaticArena(arena, arena->alignment_);
  }
  if (arena->position_ + bytes > arena->total_size_) {
    return NULL;
  }
  uint8_t* ptr = arena->memory_ + arena->position_;
  arena->position_ += bytes;
  return ptr;
}
uint8_t* Push_StaticArena(StaticArena* arena, int bytes) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  if (arena->auto_align_) {
    PushAligner_StaticArena(arena, arena->alignment_);
  }
  if (arena->position_ + bytes > arena->total_size_) {
    return NULL;
  }
  uint8_t* ptr = arena->memory_ + arena->position_;
  arena->position_ += bytes;
  memset(ptr, 0, bytes);
  return ptr;
}

int Pop_StaticArena(StaticArena* arena, uintptr_t bytes) {
  // Be careful, if auto align is on, the aligner allocated bytes are unseen to you. You should use pop to position or address if autoalign
  // is on.
#ifdef DEBUG
  if (arena == NULL) {
    return -1;
  }
#endif
  if (arena->position_ < bytes) {
    bytes = arena->position_;
  }
  arena->position_ -= bytes;
  return 0;
}
int PopTo_StaticArena(StaticArena* arena, uintptr_t position) {
#ifdef DEBUG
  if (arena == NULL) {
    return -1;
  }
#endif
  if (position < arena->position_) {
    arena->position_ = position;
  }
  return 0;
}
int PopToAdress_StaticArena(StaticArena* arena, uint8_t* address) {
#ifdef DEBUG
  if (arena == NULL) {
    return -1;
  }
#endif
  uintptr_t final_position = address - arena->memory_;
  if ((uintptr_t)(arena->memory_) < (uintptr_t)address) {
    arena->position_ = final_position;
  }
  return 0;
}
int Clear_StaticArena(StaticArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return -1;
  }
#endif
  arena->position_ = 0;
  return 0;
}

// Essentially, the scratch space is another arena of the same type rooted at the top pointer. Only works for static I guess.
int InitScratch_StaticArena(StaticArena* scratch_space, StaticArena* arena, int arena_size, int auto_align) {
#ifdef DEBUG
  if (scratch_space == NULL || scratch_space == NULL) {
    return -1;
  }
#endif
  uint8_t* mem = PushNoZero_StaticArena(arena, arena_size);
  if (mem == NULL) {
    return -1;
  }

  scratch_space->memory_ = mem;
  if (scratch_space->memory_ == NULL) {
    return -1;
  }
  scratch_space->total_size_ = arena_size;
  scratch_space->position_   = 0;
  int word_size              = WORD_SIZE;
  if (auto_align > word_size && __builtin_popcount(auto_align) == 1) {
    scratch_space->auto_align_ = TRUE;
    scratch_space->alignment_  = auto_align;
  } else {
    scratch_space->auto_align_ = FALSE;
    scratch_space->alignment_  = word_size;
  }
  return scratch_space;
}
int DestroyScratch_StaticArena(StaticArena* scratch_space, StaticArena* parent_arena) {
  // Destructor must run under locked mutex of parent to make sure of correct behaviour.
  // Check for position overflow in the memory pop.
  if (parent_arena->position_ < scratch_space->total_size_) {
    parent_arena->position_ = scratch_space->total_size_;
  }
  // Null properties and pop memory
  parent_arena->position_ -= scratch_space->total_size_;
  scratch_space->memory_     = NULL;
  scratch_space->total_size_ = 0;
  scratch_space->position_   = 0;
  scratch_space->auto_align_ = 0;
  scratch_space->alignment_  = 0;
  return 0;
}
int MergeScratch_StaticArena(StaticArena* scratch_space, StaticArena* parent_arena) {
  // Set the new position to conserve the memory from the scratch space and null properties
  // No need to do bounds check as the memory addresses must be properly ordered, and the position too.
  parent_arena->position_    = ((uintptr_t)scratch_space->memory_ - (uintptr_t)parent_arena->memory_) + scratch_space->position_;
  scratch_space->memory_     = NULL;
  scratch_space->total_size_ = 0;
  scratch_space->position_   = 0;
  scratch_space->auto_align_ = 0;
  scratch_space->alignment_  = 0;
  return 0;
}

#endif