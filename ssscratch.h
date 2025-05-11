
#ifndef _SUPER_SAFE_SCRATCH_HEADER
#define _SUPER_SAFE_SCRATCH_HEADER

#include <stdint.h>
// #include <stdlib.h>
#include "./utils.h"

typedef struct SuperSafeScratch {
    uint8_t*  memory_;
    uintptr_t total_size_;
    uintptr_t position_;
} SuperSafeScratch;

int Create_SuperSafeScratch(SuperSafeScratch* scratch, uintptr_t total_size) {
  // Error code NULL if memory failed to allocate
#ifdef DEBUG
  if (total_size < _getPageSize()) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  uintptr_t total_size = _align_2pow_ceil(total_size, _getPageSize());
  uint8_t*  mem        = _os_new_virtual_mapping_commit(total_size);
  if (mem == NULL) {
    return NULL;
  }
  scratch->memory_     = mem;
  scratch->total_size_ = total_size;
  scratch->position_   = 0;
  _os_protect_readonly(mem, total_size);
  return SUCCESS;
}

int Destroy_SuperSafeScratch(SuperSafeScratch* scratch) {
#ifdef DEBUG
  if (block == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  uint8_t*  mem        = scratch->memory_;
  uintptr_t total_size = scratch->total_size_;
  scratch->memory_     = NULL;
  scratch->total_size_ = 0;
  scratch->position_   = 0;
  _os_protect_readwrite(mem, total_size);
  if (_os_free(mem, total_size) == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Freeing old virtual memory did not work during remap. Memory leaked.");
  }
  return SUCCESS;
}
uint8_t* PushNoZero_SuperSafeScratch(SuperSafeScratch* arena, uintptr_t bytes) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  if (arena->position_ + bytes > arena->total_size_) {
    DEBUG_PRINT("Requested too much memory from SSScratch.");
    return NULL;
  }
  uint8_t* mem = arena->memory_ + arena->position_;
  arena->position_ += bytes;
  return mem;
}
uint8_t* Push_SuperSafeScratch(SuperSafeScratch* arena, uintptr_t bytes) {
#ifdef DEBUG
  if (arena == NULL) {
    return NULL;
  }
#endif
  if (arena->position_ + bytes > arena->total_size_) {
    DEBUG_PRINT("Allocating in a large memory block.");
    uint8_t* mem = PushLargeBlock_SuperSafeScratch(arena, bytes);
    if (mem == NULL) {
      return NULL;
    }
    memset(mem, 0, bytes);
    return mem;
  }
  uint8_t* mem = arena->memory_ + arena->position_;
  arena->position_ += bytes;
  _os_protect_readwrite(mem, total_size);
  memset(mem, 0, bytes);
  _os_protect_none(mem, total_size);
  return mem;
}

int Pop_SuperSafeScratch(SuperSafeScratch* arena, uintptr_t bytes) {
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
int PopTo_SuperSafeScratch(SuperSafeScratch* arena, uintptr_t position) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  if (position < arena->position_) {
    // Works because it is zero based!
    arena->position_ = position;
#ifdef DEBUG
  } else {
    return ERROR_INVALID_PARAMS;
#endif
  }
  return SUCCESS;
}
int PopToAdress_SuperSafeScratch(SuperSafeScratch* arena, uint8_t* address) {
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
int PopLargeBlock_SuperSafeScratch(SuperSafeScratch* arena) {
  arena->blocks_ = _Pop_LargeMemoryBlock(arena->blocks_);
  return SUCCESS;
}

int Clear_SuperSafeScratch(SuperSafeScratch* arena) {
#ifdef DEBUG
  if (arena == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  arena->position_ = 0;
  _DestroyAll_LargeMemBlocks(arena->blocks_);
  return SUCCESS;
}

#endif