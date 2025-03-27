
#ifndef _LINKEDV_block_HEADER
#define _LINKEDV_block_HEADER
// #include <pthread.h>
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
typedef struct LargeMemBlock {
    uint8_t*              memory_;
    uintptr_t             block_size_;
    uintptr_t             header_size_;
    struct LargeMemBlock* next_block_;
} LargeMemBlock;

LargeMemBlock* Create_LargeMemBlock(int block_size, LargeMemBlock* next_block) {
  // Error code NULL if memory failed to allocate
#ifdef DEBUG
  if (block_size < _getPageSize()) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  uintptr_t total_size = align_2pow(align_2pow(block_size, _getPageSize()) + sizeof(LargeMemBlock), _getPageSize());
  uint8_t*  mem        = os_new_virtual_mapping_commit(total_size);
  if (mem == NULL) {
    return NULL;
  }
  LargeMemBlock* block = (LargeMemBlock*)mem;
  block->block_size_   = block_size;
  block->header_size_  = total_size - block_size;
  block->memory_       = mem + block->header_size_;
  block->next_block_   = next_block;
  os_protect_readonly(mem, block->header_size_);
  return block;
}

LargeMemBlock* Pop_LargeMemoryBlock(LargeMemBlock* block) {
#ifdef DEBUG
  if (block == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  LargeMemBlock* next_block  = block->next_block_;
  uint8_t*       mem         = (uint8_t*)block;
  uintptr_t      block_size  = block->block_size_;
  uintptr_t      header_size = block->header_size_;
  os_protect_readwrite(block, block->header_size_);
  block->memory_      = NULL;
  block->block_size_  = 0;
  block->header_size_ = 0;
  block->next_block_  = NULL;

  if (os_free_(mem, block_size + header_size) == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Freeing old virtual memory did not work during remap. Memory leaked.");
  }
  return next_block;
}

int Destroy_LargeMemBlocks(LargeMemBlock* block) {
#ifdef DEBUG
  if (block == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  if (block->next_block_ != NULL) {
    if (Destroy_LargeMemBlocks(block->next_block_) == ERROR_INVALID_PARAMS) {
      DEBUG_PRINT("Bad params in destructor loop");
    }
  }
  uint8_t*  mem         = (uint8_t*)block;
  uintptr_t block_size  = block->block_size_;
  uintptr_t header_size = block->header_size_;
  os_protect_readwrite(block, header_size);
  block->memory_      = NULL;
  block->block_size_  = 0;
  block->header_size_ = 0;
  block->next_block_  = NULL;
  if (os_free_(mem, block_size + header_size) == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Freeing old virtual memory did not work during remap. Memory leaked.");
  }
  return SUCCESS;
}

LargeMemBlock* Merge_LargeMemBlocks(LargeMemBlock* first, LargeMemBlock* second) {
  if (NULL == second) {
    // If both are NULL, return is NULL
    return first;
  }
  if (NULL == first) {
    return second;
  }
  // Do not use the second pointer afterwards!
  LargeMemBlock* traversed = first;
  while (traversed->next_block_ != NULL) {
    traversed = traversed->next_block_;
  }

  os_protect_readwrite(traversed, traversed->header_size_);
  traversed->next_block_ = second;
  os_protect_readonly(traversed, traversed->header_size_);
  return first;
}

#endif