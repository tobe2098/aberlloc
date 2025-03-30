#ifndef _STATIC_ARENA_HEADER
#define _STATIC_ARENA_HEADER
// #include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include "./memblock.h"
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
    LargeMemBlock* blocks_;

    uintptr_t alignment_;
    bool      auto_align_;
    // StaticArena*    __parent;
} StaticArena;

int Init_StaticArena(StaticArena* arena, uintptr_t arena_size, uintptr_t auto_align) {
#ifdef DEBUG
  if (arena == NULL || arena_size < _getPageSize()) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->total_size_ = arena_size;
  arena->position_   = 0;
  arena->blocks_     = NULL;
  // arena->__parent     = NULL;
  int word_size = WORD_SIZE;
  if (auto_align > word_size && __builtin_popcount(auto_align) == 1) {
    arena->auto_align_ = true;
    arena->alignment_  = auto_align;
  } else {
    arena->auto_align_ = false;
    arena->alignment_  = word_size;
  }
  arena->memory_ = os_new_virtual_mapping_commit(arena_size);
  if (arena->memory_ == NULL) {
    return ERROR_OS_MEMORY;
  }
  return SUCCESS;
}
int Destroy_StaticArena(StaticArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  Destroy_LargeMemBlocks(arena->blocks_);
  if (os_free_(arena->memory_, arena->total_size_) == ERROR_OS_MEMORY) {
    return ERROR_OS_MEMORY;
  }
  arena->memory_     = NULL;
  arena->total_size_ = 0;
  arena->position_   = 0;
  arena->auto_align_ = 0;
  return SUCCESS;
}

int SetAutoAlign2Pow_StaticArena(StaticArena* arena, uintptr_t alignment, bool auto_align) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->auto_align_ = auto_align;
  if (!auto_align) {
    return SUCCESS;
  }
  arena->alignment_ = alignment;
  return SUCCESS;
}

inline uintptr_t GetPos_StaticArena(StaticArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  return arena->position_;
}

int PushAligner_StaticArena(StaticArena* arena, uintptr_t alignment) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = align_2pow(arena->position_, alignment);
  return SUCCESS;
}

int PushAlignerCacheLine_StaticArena(StaticArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = align_2pow(arena->position_, CACHE_LINE_SIZE);
  return SUCCESS;
}
int PushAlignerPageSize_StaticArena(StaticArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = align_2pow(arena->position_, _getPageSize());
  return SUCCESS;
}
uint8_t* PushLargeBlock_StaticArena(StaticArena* arena, uintptr_t bytes) {
  DEBUG_PRINT("Large block allocation of %d", bytes);
  LargeMemBlock* new_block = Create_LargeMemBlock(bytes, arena->blocks_);
  if (new_block == NULL) {
    DEBUG_PRINT("Failed large block memory allocation");
    return NULL;
  }
  arena->blocks_ = new_block;
  return new_block->memory_;
}
uint8_t* PushNoZero_StaticArena(StaticArena* arena, uintptr_t bytes) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  if (arena->auto_align_) {
    PushAligner_StaticArena(arena, arena->alignment_);
  }
  if (arena->position_ + bytes > arena->total_size_) {
    return PushLargeBlock_StaticArena(arena, bytes);
  }
  uint8_t* ptr = arena->memory_ + arena->position_;
  arena->position_ += bytes;
  return ptr;
}
uint8_t* Push_StaticArena(StaticArena* arena, uintptr_t bytes) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  if (arena->auto_align_) {
    PushAligner_StaticArena(arena, arena->alignment_);
  }
  if (arena->position_ + bytes > arena->total_size_) {
    uint8_t* mem = PushLargeBlock_StaticArena(arena, bytes);
    memset(mem, 0, bytes);
    return mem;
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
    return ERROR_INVALID_PARAMS;
  }
#endif
  if (arena->position_ < bytes) {
    bytes = arena->position_;
  }
  arena->position_ -= bytes;
  return SUCCESS;
}
int PopTo_StaticArena(StaticArena* arena, uintptr_t position) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  if (position < arena->position_) {
    arena->position_ = position;
  }
  return SUCCESS;
}
int PopToAdress_StaticArena(StaticArena* arena, uint8_t* address) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  uintptr_t final_position = address - arena->memory_;
  if ((uintptr_t)(arena->memory_) < (uintptr_t)address) {
    arena->position_ = final_position;
  }
  return SUCCESS;
}
int PopLargeBlock_StaticArena(StaticArena* arena) {
  arena->blocks_ = Pop_LargeMemoryBlock(arena->blocks_);
  return SUCCESS;
}

int Clear_StaticArena(StaticArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = 0;
  Destroy_LargeMemBlocks(arena->blocks_);
  return SUCCESS;
}

// Essentially, the scratch space is another arena of the same type rooted at the top pointer. Only works for static I guess.
int InitScratch_StaticArena(StaticArena* scratch_space, StaticArena* parent_arena, uintptr_t arena_size, uintptr_t auto_align) {
#ifdef DEBUG
  if (scratch_space == NULL || scratch_space == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  uint8_t* mem = PushNoZero_StaticArena(parent_arena, arena_size);
  if (mem == NULL) {
    return ERROR_OS_MEMORY;
  }

  scratch_space->memory_ = mem;

  scratch_space->total_size_ = arena_size;
  scratch_space->position_   = 0;
  scratch_space->blocks_     = NULL;
  int word_size              = WORD_SIZE;
  if (auto_align > word_size && __builtin_popcount(auto_align) == 1) {
    scratch_space->auto_align_ = TRUE;
    scratch_space->alignment_  = auto_align;
  } else {
    scratch_space->auto_align_ = FALSE;
    scratch_space->alignment_  = word_size;
  }
  return SUCCESS;
}
int DestroyScratch_StaticArena(StaticArena* scratch_space, StaticArena* parent_arena) {
  // Destructor must run under locked mutex of parent to make sure of correct behaviour.
  // Check for position overflow in the memory pop.
  if (parent_arena->position_ < scratch_space->total_size_) {
    parent_arena->position_ = scratch_space->total_size_;
  }
  // Null properties and pop memory
  parent_arena->position_ -= scratch_space->total_size_;
  scratch_space->memory_ = NULL;

  Destroy_LargeMemBlocks(scratch_space->blocks_);
  scratch_space->blocks_ = NULL;

  scratch_space->total_size_ = 0;
  scratch_space->position_   = 0;
  scratch_space->auto_align_ = 0;
  scratch_space->alignment_  = 0;
  return SUCCESS;
}
int MergeScratch_StaticArena(StaticArena* scratch_space, StaticArena* parent_arena) {
  // Set the new position to conserve the memory from the scratch space and null properties
  // No need to do bounds check as the memory addresses must be properly ordered, and the position too.
  parent_arena->position_ = ((uintptr_t)scratch_space->memory_ - (uintptr_t)parent_arena->memory_) + scratch_space->position_;
  scratch_space->memory_  = NULL;

  parent_arena->blocks_  = Merge_LargeMemBlocks(scratch_space->blocks_, parent_arena->blocks_);
  scratch_space->blocks_ = NULL;

  scratch_space->total_size_ = 0;
  scratch_space->position_   = 0;
  scratch_space->auto_align_ = 0;
  scratch_space->alignment_  = 0;
  return SUCCESS;
}

#endif