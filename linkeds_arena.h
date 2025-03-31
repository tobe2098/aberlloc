#ifndef _LINKEDS_ARENA_HEADER
#define _LINKEDS_ARENA_HEADER
// #include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include "./memblock.h"
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
typedef struct LinkedSArena {
    uint8_t* memory_;    // Base pointer to reserved memory
    uint8_t* base_ptr_;  // Current allocation position

    uintptr_t position_;  // Current allocation position
    uintptr_t add_committed_size_;
    uintptr_t total_size_;  // Size
    uintptr_t base_block_size_;
    uintptr_t usable_size_;
    // pthread_mutex_t __arena_mutex;
    LinkedSArena*   next_arena_;
    _LargeMemBlock* blocks_;

    uintptr_t alignment_;
    bool      auto_align_;
    bool      newblock_pagealign_;

} LinkedSArena;

int Init_LinkedSArena(LinkedSArena* arena, uintptr_t arena_size, bool auto_align, bool newblock_pagealign) {
#ifdef DEBUG
  if (arena == NULL || arena_size < _getPageSize()) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->base_block_size_ = _align_2pow(arena_size, _getPageSize());
  arena->total_size_      = arena->base_block_size_ + _align_2pow(sizeof(LinkedSArena), _getPageSize());
  arena->position_        = 0;

  // arena->__parent     = NULL;
  int word_size              = WORD_SIZE;
  arena->newblock_pagealign_ = newblock_pagealign;
  if (newblock_pagealign) {
    arena->base_ptr_    = arena->memory_ + (arena->total_size_ - arena->base_block_size_);
    arena->usable_size_ = arena->base_block_size_;
  } else {
    arena->base_ptr_    = arena->memory_ + sizeof(LinkedSArena);
    arena->usable_size_ = arena->total_size_ - sizeof(LinkedSArena);
  }
  arena->blocks_ = NULL;
  if (auto_align > word_size && __builtin_popcount(auto_align) == 1) {
    arena->auto_align_ = true;
    arena->alignment_  = auto_align;
  } else {
    arena->auto_align_ = false;
    arena->alignment_  = word_size;
  }
  arena->add_committed_size_ = _getPageSize();
  arena->memory_             = os_new_virtual_mapping_(arena->total_size_);
  arena->next_arena_         = NULL;
  if (arena->memory_ == NULL) {
    return ERROR_OS_MEMORY;
  }
  if (_os_commit(arena->memory_, arena->total_size_ - arena->base_block_size_ + arena->add_committed_size_) == ERROR_OS_MEMORY) {
    _os_free(arena->memory_, arena->total_size_);
    return ERROR_OS_MEMORY;
  }
  return SUCCESS;
}
// Here
int Destroy_LinkedSArena(LinkedSArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  if (arena->blocks_ != NULL) {
    _DestroyAll_LargeMemBlocks(arena->blocks_);
  }
  if (arena->next_arena_ != NULL) {
    if (Destroy_LinkedSArena(arena) == ERROR_INVALID_PARAMS) {
      DEBUG_PRINT("Bad params in destructor loop.");
    }
  }
  if (_os_free(arena->memory_, arena->total_size_) == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Freeing old virtual memory did not work during remap. Memory leaked.");
  }

  arena->memory_             = NULL;
  arena->base_ptr_           = NULL;
  arena->usable_size_        = 0;
  arena->total_size_         = 0;
  arena->add_committed_size_ = 0;
  arena->position_           = 0;
  arena->auto_align_         = 0;
  arena->alignment_          = 0;
  arena->next_arena_         = NULL;
  arena->blocks_             = NULL;
  return SUCCESS;
}

int SetAutoAlign2Pow_LinkedSArena(LinkedSArena* arena, uintptr_t alignment, bool auto_align) {
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
int Set_NewBlock_PageAlign(LinkedSArena* arena, bool boolean_val) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->newblock_pagealign_ = boolean_val;
  return SUCCESS;
}

uint8_t* PushLargeBlock_LinkedSArena(LinkedSArena* arena, uintptr_t bytes) {
  DEBUG_PRINT("Large block allocation of %d.", bytes);
  _LargeMemBlock* new_block = _Create_LargeMemBlock(bytes, arena->blocks_);
  if (new_block == NULL) {
    DEBUG_PRINT("Failed large block memory allocation.");
    return NULL;
  }
  arena->blocks_ = new_block;
  return new_block->memory_ + new_block->header_size_;
}

int NewBlock_LinkedSArena(LinkedSArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    // Need to ensure there is enough space at destination of memcopy
    return ERROR_INVALID_PARAMS;
  }
#endif
  // We store the old parent arena
  LinkedSArena temp = *arena;
  // Reset the parent arena wiht new memblock of same size
  int err_code = Init_LinkedSArena(arena, arena->base_block_size_, arena->alignment_, arena->newblock_pagealign_);
  if (err_code == ERROR_INVALID_PARAMS) {
    DEBUG_PRINT("Bad params in NewBlock.");
    return ERROR_INVALID_PARAMS;
  } else if (err_code == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Could not acquire enough memory.");
    return ERROR_OS_MEMORY;
  }
  // Allocate memory for old block tracking in new block
  LinkedSArena* storage_old_block = (LinkedSArena*)Push_LinkedSArena(arena, sizeof(LinkedSArena));
  // Page boundary alignment to avoid performance issues in the user side
  if (arena->newblock_pagealign_ == true) {
    PushAlignerPageSize_LinkedSArena(arena);
  }
  // Copy the data (including pointers) of old parent block into the allocated block
  memcpy(storage_old_block, &temp, sizeof(LinkedSArena));
  // Set the next pointer for destructor reasons.
  arena->next_arena_         = storage_old_block;
  arena->blocks_             = temp.blocks_;
  storage_old_block->blocks_ = NULL;
  return SUCCESS;
}

int ExtendCommit_LinkedSArena(LinkedSArena* arena, uintptr_t total_commited_size) {
#ifdef DEBUG
  if (!arena || !total_commited_size) {
    return ERROR_INVALID_PARAMS;
  }
  if (total_commited_size < arena->committed_size_) {
    DEBUG_PRINT("Possible overflow");
    return ERROR_INVALID_PARAMS;
  }
  if (total_commited_size > arena->base_block_ && arena->base_block_ == arena->committed_size_) {
    DEBUG_PRINT("ExtendCommit should not have been called under these conditions.")
    return ERROR_INVALID_PARAMS;
  }
#endif
  // if (total_commited_size > arena->base_block_ && arena->base_block_ == arena->committed_size_) {
  //   // Nothing, we shouldnt call this function in the first place in these conditions
  // } else
  if (total_commited_size > arena->base_block_size_) {
    total_commited_size = arena->base_block_size_;
  }

  if (total_commited_size == arena->add_committed_size_) {
    DEBUG_PRINT("Not enough virtual memory in the block, creating a new one.");
    if (NewBlock_LinkedSArena(arena) != SUCCESS) {
      DEBUG_PRINT("Block creation failed");
      return ERROR_OS_MEMORY;
    }
  } else {
    if (_os_commit(arena->memory_, arena->total_size_ - arena->base_block_size_ + total_commited_size) == ERROR_OS_MEMORY) {
      return ERROR_OS_MEMORY;
    }
    arena->add_committed_size_ = total_commited_size;
  }
  // We only need to extend the memory commitment under the total size.

  return SUCCESS;
}
int ReduceCommit_LinkedSArena(LinkedSArena* arena, uintptr_t total_commited_size) {
#ifdef DEBUG
  if (!arena || !total_commited_size || total_commited_size > arena->__commited_size) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  if (_os_uncommit(arena->memory_ + (arena->total_size_ - arena->base_block_size_) + total_commited_size,
                   arena->add_committed_size_ - total_commited_size) == ERROR_OS_MEMORY) {
    return ERROR_OS_MEMORY;
  }
  arena->add_committed_size_ = total_commited_size;
}
uintptr_t GetPos_LinkedSArena(LinkedSArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  return arena->position_;
}

int PushAligner_LinkedSArena(LinkedSArena* arena, uintptr_t alignment) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
  if (__builtin_popcount(alignment) != 1 || alignment < WORD_SIZE) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = _align_2pow(arena->position_ + (uintptr_t)arena->base_ptr_, alignment) - (uintptr_t)arena->base_ptr_;
  // arena->position_ = align_2pow(arena->position_ + (uintptr_t)arena->__memory, alignment) - (uintptr_t)arena->__memory;
  return SUCCESS;
}

int PushAlignerCacheLine_LinkedSArena(LinkedSArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = _align_2pow(arena->position_ + (uintptr_t)arena->base_ptr_, CACHE_LINE_SIZE) - (uintptr_t)arena->base_ptr_;
  return SUCCESS;
}

int PushAlignerPageSize_LinkedSArena(LinkedSArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = _align_2pow(arena->position_ + (uintptr_t)arena->base_ptr_, _getPageSize()) - (uintptr_t)arena->base_ptr_;
  return SUCCESS;
}

uint8_t* PushNoZero_LinkedSArena(LinkedSArena* arena, uintptr_t bytes) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  if (arena->auto_align_) {
    PushAligner_LinkedSArena(arena, arena->alignment_);
  }
  if (bytes <= arena->base_block_size_ / 2) {
    // To avoid wasted memory, we estimate
    while (arena->position_ + bytes > arena->add_committed_size_ + (arena->usable_size_ - arena->base_block_size_)) {
      if (ExtendCommit_LinkedSArena(arena, extendPolicy(arena->add_committed_size_)) == ERROR_OS_MEMORY) {
        return NULL;
      }
    }
  } else {
    DEBUG_PRINT("Allocating in a large memory block.");
    return PushLargeBlock_LinkedSArena(arena, bytes);
  }
  uint8_t* mem = arena->base_ptr_ + arena->position_;
  arena->position_ += bytes;
  return mem;
}
uint8_t* Push_LinkedSArena(LinkedSArena* arena, uintptr_t bytes) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  if (arena->auto_align_) {
    PushAligner_LinkedSArena(arena, arena->alignment_);
  }

  if (bytes <= arena->base_block_size_ / 2) {
    // To avoid wasted memory, we estimate
    while (arena->position_ + bytes > arena->add_committed_size_ + (arena->usable_size_ - arena->base_block_size_)) {
      if (ExtendCommit_LinkedSArena(arena, extendPolicy(arena->add_committed_size_)) == ERROR_OS_MEMORY) {
        return NULL;
      }
    }
  } else {
    DEBUG_PRINT("Allocating in a large memory block.");
    uint8_t* mem = PushLargeBlock_LinkedSArena(arena, bytes);
    memset(mem, 0, bytes);
    return mem;
  }
  uint8_t* mem = arena->base_ptr_ + arena->position_;
  arena->position_ += bytes;
  memset(mem, 0, bytes);
  return mem;
}

int Pop_LinkedSArena(LinkedSArena* arena, uintptr_t bytes) {
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
  while (arena->position_ > _getPageSize() && reduceCondition(arena->position_, arena->add_committed_size_)) {
    if (ReduceCommit_LinkedSArena(arena, reducePolicy(arena->add_committed_size_)) == ERROR_OS_MEMORY) {
      DEBUG_PRINT("Reduce commit in Virtual arena failed");
    }
  }
  return SUCCESS;
}
int PopTo_LinkedSArena(LinkedSArena* arena, uintptr_t position) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  if (position < arena->position_) {
    arena->position_ = position;
  }
  while (arena->position_ > _getPageSize() && reduceCondition(arena->position_, arena->add_committed_size_)) {
    if (ReduceCommit_LinkedSArena(arena, reducePolicy(arena->add_committed_size_)) == ERROR_OS_MEMORY) {
      DEBUG_PRINT("Reduce commit in Virtual arena failed");
    }
  }
  return SUCCESS;
}
int PopToAdress_LinkedSArena(LinkedSArena* arena, uint8_t* address) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  uintptr_t final_position = address - arena->base_ptr_;
  if ((uintptr_t)(arena->base_ptr_) < (uintptr_t)address && (uintptr_t)(arena->base_ptr_) + arena->position_ > (uintptr_t)address) {
    arena->position_ = final_position;
  } else {
    DEBUG_PRINT("Address argument is outside the memory in use : PopToAddress");
  }
  while (arena->position_ > _getPageSize() && reduceCondition(arena->position_, arena->add_committed_size_)) {
    if (ReduceCommit_LinkedSArena(arena, reducePolicy(arena->add_committed_size_)) == ERROR_OS_MEMORY) {
      DEBUG_PRINT("Reduce commit in Virtual arena failed");
    }
  }
  return SUCCESS;
}

int PopBlock_LinkedSArena(LinkedSArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  // We copy the current arena
  LinkedSArena temp = *arena;
  // We copy the old block to the parent block
  memcpy(arena, arena->next_arena_, sizeof(LinkedSArena));
  // Break the link with the block chain
  temp.next_arena_ = NULL;
  // Destroy the isolated block
  if (Destroy_LinkedSArena(&temp) == ERROR_INVALID_PARAMS) {
    DEBUG_PRINT("Bad parameters in destructor");
  }
  return SUCCESS;
}

int PopLargeBlock_LinkedSArena(LinkedSArena* arena) {
  arena->blocks_ = _Pop_LargeMemoryBlock(arena->blocks_);
  return SUCCESS;
}

int ClearCurrentBlock_LinkedSArena(LinkedSArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = 0;
  if (ReduceCommit_LinkedSArena(arena, _getPageSize()) == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Reduce commit in Virtual arena failed");
  }
  return SUCCESS;
}

int ClearAll_LinkedSArena(LinkedSArena* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = 0;
  if (arena->next_arena_ != NULL && Destroy_LinkedSArena(arena->next_arena_) == ERROR_INVALID_PARAMS) {
    DEBUG_PRINT("Bad params in destructor");
  }
  arena->next_arena_ = NULL;
  if (arena->blocks_ != NULL) {
    if (_DestroyAll_LargeMemBlocks(arena->blocks_) == ERROR_INVALID_PARAMS) {
      DEBUG_PRINT("Bad params in destructor");
    }
  }
  if (ReduceCommit_LinkedSArena(arena, _getPageSize()) == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Reduce commit in Virtual arena failed");
  }
  return SUCCESS;
}

// Essentially, the scratch space is another arena of the same type rooted at the top pointer. Only works for static I guess.
int InitScratch_LinkedSArena(StaticArena* scratch_space, LinkedSArena* arena, uintptr_t arena_size, uintptr_t auto_align) {
#ifdef DEBUG
  if (scratch_space == NULL || scratch_space == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  uint8_t* mem = PushNoZero_LinkedSArena(arena, arena_size);
  if (mem == NULL) {
    return ERROR_OS_MEMORY;
  }

  scratch_space->memory_ = mem;

  scratch_space->total_size_ = arena_size;
  scratch_space->position_   = 0;
  int word_size              = WORD_SIZE;
  if (auto_align > word_size && __builtin_popcount(auto_align) == 1) {
    scratch_space->auto_align_ = true;
    scratch_space->alignment_  = auto_align;
  } else {
    scratch_space->auto_align_ = false;
    scratch_space->alignment_  = word_size;
  }
  return SUCCESS;
}
int DestroyScratch_LinkedSArena(StaticArena* scratch_space, LinkedSArena* parent_arena) {
  // Make sure you destroy arenas in reverse order on which you created them for correctness.
  // Check for position overflow in the memory pop.
  LinkedSArena* traverse = parent_arena;
  bool          found    = false;
  while (traverse) {
    if (parent_arena->memory_ <= scratch_space->memory_ && parent_arena->memory_ + parent_arena->total_size_ > scratch_space->memory_) {
      found = true;
      break;
    }
    traverse->next_arena_;
  }
  if (found) {
    if (parent_arena != traverse ||
        scratch_space->memory_ != parent_arena->memory_ + parent_arena->position_ - scratch_space->total_size_) {
      DEBUG_PRINT("Cannot delete the used memory of the scratch because it is not on the top of the arena.");
      return ERROR_INVALID_PARAMS;
    }
  } else {
    if (_DeleteSingle_LargeMemBlock(parent_arena->blocks_, scratch_space->memory_) == ERROR_INVALID_PARAMS) {
      DEBUG_PRINT("The scratch space was not found.");
      return ERROR_INVALID_PARAMS;
    }
    return SUCCESS;
  }
  // if (parent_arena->position_ < scratch_space->total_size_) {
  //   parent_arena->position_ = scratch_space->total_size_;
  // }
  // Null properties and pop memory
  parent_arena->position_ -= scratch_space->total_size_;
  scratch_space->memory_     = NULL;
  scratch_space->total_size_ = 0;
  scratch_space->position_   = 0;
  scratch_space->auto_align_ = 0;
  scratch_space->alignment_  = 0;
  return SUCCESS;
}
int MergeScratch_LinkedSArena(StaticArena* scratch_space, LinkedSArena* parent_arena) {
  // Merger must run under locked mutex of parent to make sure of correct behaviour.
  // Set the new position to conserve the memory from the scratch space and null properties
  // No need to do bounds check as the memory addresses must be properly ordered, and the position too.
  LinkedSArena* traverse = parent_arena;
  bool          found    = false;
  while (traverse) {
    if (parent_arena->memory_ <= scratch_space->memory_ && parent_arena->memory_ + parent_arena->total_size_ > scratch_space->memory_) {
      found = true;
      break;
    }
    traverse->next_arena_;
  }
  if (found) {
    if (parent_arena != traverse ||
        scratch_space->memory_ != parent_arena->memory_ + parent_arena->position_ - scratch_space->total_size_) {
      DEBUG_PRINT("Cannot merge the used memory of the scratch because it is not on the top of the arena.");
      return ERROR_INVALID_PARAMS;
    }
  } else {
    DEBUG_PRINT("Cannot merge a scratch space that is allocated in an isolated block.");
    return ERROR_INVALID_PARAMS;
  }
  parent_arena->position_    = ((uintptr_t)scratch_space->memory_ - (uintptr_t)parent_arena->memory_) + scratch_space->position_;
  scratch_space->memory_     = NULL;
  scratch_space->total_size_ = 0;
  scratch_space->position_   = 0;
  scratch_space->auto_align_ = 0;
  scratch_space->alignment_  = 0;
  return SUCCESS;
}

#endif