#ifndef _LINKEDV_ARENA_HEADER
#define _LINKEDV_ARENA_HEADER
// #include <pthread.h>
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
typedef struct LinkedVArena {
    uint8_t*  memory_;    // Base pointer to reserved memory
    uintptr_t position_;  // Current allocation position
    uintptr_t committed_size_;
    uintptr_t total_size_;  // Size
    // pthread_mutex_t __arena_mutex;
    int auto_align_;
    int alignment_;

    LinkedVArena* next_arena_;
} LinkedVArena;

int Init_ParentLinkedVArena(LinkedVArena* arena, int arena_size, int auto_align) {
#ifdef DEBUG
  if (arena == NULL || arena_size < _getPageSize()) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->total_size_ = align_2pow(arena_size + sizeof(LinkedVArena), _getPageSize());
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
  arena->committed_size_ = _getPageSize();
  arena->memory_         = os_new_virtual_mapping_(arena->total_size_);
  arena->next_arena_     = NULL;
  if (arena->memory_ == NULL) {
    return ERROR_OS_MEMORY;
  }
  if (os_commit_(arena->memory_, arena->committed_size_) == ERROR_OS_MEMORY) {
    os_free_(arena->memory_, arena->total_size_);
    return ERROR_OS_MEMORY;
  }
  return SUCCESS;
}
// Here
int Destroy_LinkedVArena(LinkedVArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  while (arena->next_arena_ != NULL) {
    if (Destroy_LinkedVArena(arena) == ERROR_INVALID_PARAMS) {
      DEBUG_PRINT("Bad params in destructor loop");
    }
  }
  if (os_free_(arena->memory_, arena->total_size_) == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Freeing old virtual memory did not work during remap. Memory leaked.");
  }

  arena->memory_         = NULL;
  arena->total_size_     = 0;
  arena->committed_size_ = 0;
  arena->position_       = 0;
  arena->auto_align_     = 0;
  arena->alignment_      = 0;
  arena->next_arena_     = NULL;
  return SUCCESS;
}

int SetAutoAlign2Pow_LinkedVArena(LinkedVArena* arena, int alignment) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->auto_align_ = TRUE;
  arena->alignment_  = alignment;
  return SUCCESS;
}

int NewBlock_LinkedVArena(LinkedVArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    // Need to ensure there is enough space at destination of memcopy
    return ERROR_INVALID_PARAMS;
  }
#endif
  // We store the old parent arena
  LinkedVArena temp = *arena;
  // Reset the parent arena wiht new memblock of same size
  int err_code = Init_ParentLinkedVArena(arena, arena->total_size_ - sizeof(LinkedVArena), arena->alignment_);
  if (err_code == ERROR_INVALID_PARAMS) {
    DEBUG_PRINT("Bad params in NewBlock");
  } else if (err_code == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Could not acquire enough memory");
  }
  // Allocate memory for old block tracking in new block
  LinkedVArena* storage_old_block = (LinkedVArena*)Push_LinkedVArena(arena, sizeof(LinkedVArena));
  // Page boundary alignment to avoid performance issues in the user side
  PushAlignerPageSize_LinkedVArena(arena);
  // Copy the data (including pointers) of old parent block into the allocated block
  memcpy(storage_old_block, &temp, sizeof(LinkedVArena));
  // Set the next pointer for destructor reasons.
  arena->next_arena_ = storage_old_block;
  return SUCCESS;
}

int ExtendCommit_LinkedVArena(LinkedVArena* arena, int total_commited_size) {
#ifdef DEBUG
  if (!arena || !total_commited_size || total_commited_size < arena->committed_size_) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  if (total_commited_size > arena->total_size_) {
    DEBUG_PRINT("Not enough virtual memory in the arena, remapping.");
    if (!NewBlock_LinkedVArena(arena)) {
      DEBUG_PRINT("Remap failed, not enough memory.");
      return ERROR_OS_MEMORY;
    }
  }
  // We only need to extend the memory commitment under the total size.
  if (os_commit_(arena->memory_, total_commited_size) == ERROR_OS_MEMORY) {
    return ERROR_OS_MEMORY;
  }
  arena->committed_size_ = total_commited_size;
}
int ReduceCommit_LinkedVArena(LinkedVArena* arena, int total_commited_size) {
#ifdef DEBUG
  if (!arena || !total_commited_size || total_commited_size > arena->__commited_size) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  if (os_uncommit_(arena->memory_ + total_commited_size, arena->committed_size_ - total_commited_size) == ERROR_OS_MEMORY) {
    return ERROR_OS_MEMORY;
  }
  arena->committed_size_ = total_commited_size;
}
uintptr_t GetPos_LinkedVArena(LinkedVArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  return arena->position_;
}

int PushAligner_LinkedVArena(LinkedVArena* arena, int alignment) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = align_2pow(arena->position_, alignment);
  // arena->position_ = align_2pow(arena->position_ + (uintptr_t)arena->__memory, alignment) - (uintptr_t)arena->__memory;
  return SUCCESS;
}

int PushAlignerCacheLine_LinkedVArena(LinkedVArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = align_2pow(arena->position_ + (uintptr_t)arena->memory_, CACHE_LINE_SIZE) - (uintptr_t)arena->memory_;
  return SUCCESS;
}

int PushAlignerPageSize_LinkedVArena(LinkedVArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = align_2pow(arena->position_ + (uintptr_t)arena->memory_, _getPageSize()) - (uintptr_t)arena->memory_;
  return SUCCESS;
}

uint8_t* PushNoZero_LinkedVArena(LinkedVArena* arena, int bytes) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  if (arena->auto_align_) {
    PushAligner_LinkedVArena(arena, arena->alignment_);
  }
  while (arena->position_ + bytes > arena->committed_size_) {
    if (ExtendCommit_LinkedVArena(arena, extendPolicy(arena->committed_size_)) == ERROR_OS_MEMORY) {
      return NULL;
    }
  }
  uint8_t* ptr = arena->memory_ + arena->position_;
  arena->position_ += bytes;
  return ptr;
}
uint8_t* Push_LinkedVArena(LinkedVArena* arena, int bytes) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  if (arena->auto_align_) {
    PushAligner_LinkedVArena(arena, arena->alignment_);
  }

  while (arena->position_ + bytes > arena->committed_size_) {
    if (ExtendCommit_LinkedVArena(arena, extendPolicy(arena->committed_size_)) == ERROR_OS_MEMORY) {
      return NULL;
    }
  }
  uint8_t* ptr = arena->memory_ + arena->position_;
  arena->position_ += bytes;
  memset(ptr, 0, bytes);
  return ptr;
}

int Pop_LinkedVArena(LinkedVArena* arena, uintptr_t bytes) {
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
  while (arena->position_ > _getPageSize() && reduceCondition(arena->committed_size_, arena->position_)) {
    if (ReduceCommit_LinkedVArena(arena, reducePolicy(arena->committed_size_)) == ERROR_OS_MEMORY) {
      DEBUG_PRINT("Reduce commit in Virtual arena failed");
    }
  }
  return SUCCESS;
}
int PopTo_LinkedVArena(LinkedVArena* arena, uintptr_t position) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  if (position < arena->position_) {
    arena->position_ = position;
  }
  while (arena->position_ > _getPageSize() && reduceCondition(arena->committed_size_, arena->position_)) {
    if (ReduceCommit_LinkedVArena(arena, reducePolicy(arena->committed_size_)) == ERROR_OS_MEMORY) {
      DEBUG_PRINT("Reduce commit in Virtual arena failed");
    }
  }
  return SUCCESS;
}
int PopToAdress_LinkedVArena(LinkedVArena* arena, uint8_t* address) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  uintptr_t final_position = address - arena->memory_;
  if ((uintptr_t)(arena->memory_) < (uintptr_t)address || (uintptr_t)(arena->memory_) + arena->position_ > (uintptr_t)address) {
    arena->position_ = final_position;
  } else {
    DEBUG_PRINT("Address is outside the memory in use in PopToAddress");
  }
  while (arena->position_ > _getPageSize() && reduceCondition(arena->committed_size_, arena->position_)) {
    if (ReduceCommit_LinkedVArena(arena, reducePolicy(arena->committed_size_)) == ERROR_OS_MEMORY) {
      DEBUG_PRINT("Reduce commit in Virtual arena failed");
    }
  }
  return SUCCESS;
}

int PopBlock_LinkedVArena(LinkedVArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  // We copy the current arena
  LinkedVArena temp = *arena;
  // We copy the old block to the parent block
  memcpy(arena, arena->next_arena_, sizeof(LinkedVArena));
  // Break the link with the block chain
  temp.next_arena_ = NULL;
  // Destroy the isolated block
  if (Destroy_LinkedVArena(&temp) == ERROR_INVALID_PARAMS) {
    DEBUG_PRINT("Bad parameters in destructor");
  }
  return SUCCESS;
}

int ClearBlock_LinkedVArena(LinkedVArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = 0;
  if (ReduceCommit_LinkedVArena(arena, _getPageSize()) == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Reduce commit in Virtual arena failed");
  }
  return SUCCESS;
}

int ClearAll_LinkedVArena(LinkedVArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = 0;
  if (Destroy_LinkedVArena(arena->next_arena_) == ERROR_INVALID_PARAMS) {
    DEBUG_PRINT("Bad params in destructor");
  }
  arena->next_arena_ = NULL;
  if (ReduceCommit_LinkedVArena(arena, _getPageSize()) == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Reduce commit in Virtual arena failed");
  }
  return SUCCESS;
}

// Essentially, the scratch space is another arena of the same type rooted at the top pointer. Only works for static I guess.
int InitScratch_LinkedVArena(StaticArena* scratch_space, LinkedVArena* arena, int arena_size, int auto_align) {
#ifdef DEBUG
  if (scratch_space == NULL || scratch_space == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  uint8_t* mem = PushNoZero_LinkedVArena(arena, arena_size);
  if (mem == NULL) {
    return ERROR_OS_MEMORY;
  }

  scratch_space->memory_ = mem;

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
  return SUCCESS;
}
int DestroyScratch_LinkedVArena(StaticArena* scratch_space, LinkedVArena* parent_arena) {
  // Make sure you destroy arenas in reverse order on which you created them for correctness.
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
  return SUCCESS;
}
int MergeScratch_LinkedVArena(StaticArena* scratch_space, LinkedVArena* parent_arena) {
  // Merger must run under locked mutex of parent to make sure of correct behaviour.
  // Set the new position to conserve the memory from the scratch space and null properties
  // No need to do bounds check as the memory addresses must be properly ordered, and the position too.
  parent_arena->position_    = ((uintptr_t)scratch_space->memory_ - (uintptr_t)parent_arena->memory_) + scratch_space->position_;
  scratch_space->memory_     = NULL;
  scratch_space->total_size_ = 0;
  scratch_space->position_   = 0;
  scratch_space->auto_align_ = 0;
  scratch_space->alignment_  = 0;
  return SUCCESS;
}

#endif