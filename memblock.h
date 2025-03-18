
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
    uint8_t*       memory_;
    uintptr_t      total_size_;
    LargeMemBlock* next_block_;
} LargeMemBlock;

int Init_LargeMemBlock(LargeMemBlock* block, int block_size) {
#ifdef DEBUG
  if (block == NULL || block_size < _getPageSize()) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  block->total_size_ = block_size;
  // block->__parent     = NULL;
  int word_size      = WORD_SIZE;
  block->memory_     = os_new_virtual_mapping_commit(block->total_size_);
  block->next_block_ = NULL;
  if (block->memory_ == NULL) {
    return ERROR_OS_MEMORY;
  }
  return SUCCESS;
}
// Here
int Destroy_LargeMemBlocks(LargeMemBlock* block) {
#ifdef DEBUG
  if (block == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  while (block->next_block_ != NULL) {
    if (Destroy_LargeMemBlocks(block) == ERROR_INVALID_PARAMS) {
      DEBUG_PRINT("Bad params in destructor loop");
    }
  }
  if (os_free_(block->memory_, block->total_size_) == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Freeing old virtual memory did not work during remap. Memory leaked.");
  }

  block->memory_     = NULL;
  block->total_size_ = 0;
  block->next_block_ = NULL;
  return SUCCESS;
}
#endif