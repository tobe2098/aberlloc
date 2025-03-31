
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
typedef struct _LargeMemBlock {
    uint8_t*               memory_;
    uintptr_t              block_size_;
    uintptr_t              header_size_;
    struct _LargeMemBlock* next_block_;
} _LargeMemBlock;

_LargeMemBlock* _Create_LargeMemBlock(uintptr_t block_size, _LargeMemBlock* next_block) {
  // Error code NULL if memory failed to allocate
#ifdef DEBUG
  if (block_size < _getPageSize()) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  uintptr_t total_size = _align_2pow(_align_2pow(block_size, _getPageSize()) + sizeof(_LargeMemBlock), _getPageSize());
  uint8_t*  mem        = _os_new_virtual_mapping_commit(total_size);
  if (mem == NULL) {
    return NULL;
  }
  _LargeMemBlock* block = (_LargeMemBlock*)mem;
  block->block_size_    = block_size;
  block->header_size_   = total_size - block_size;
  block->memory_        = mem + block->header_size_;
  block->next_block_    = next_block;
  _os_protect_readonly(mem, block->header_size_);
  return block;
}

int _Destroy_LargeMemBlock(_LargeMemBlock* block) {
#ifdef DEBUG
  if (block == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  uint8_t*  mem         = (uint8_t*)block;
  uintptr_t block_size  = block->block_size_;
  uintptr_t header_size = block->header_size_;
  _os_protect_readwrite(block, header_size);
  block->memory_      = NULL;
  block->block_size_  = 0;
  block->header_size_ = 0;
  block->next_block_  = NULL;
  if (_os_free(mem, block_size + header_size) == ERROR_OS_MEMORY) {
    DEBUG_PRINT("Freeing old virtual memory did not work during remap. Memory leaked.");
  }
  return SUCCESS;
}

_LargeMemBlock* _Pop_LargeMemoryBlock(_LargeMemBlock* block) {
#ifdef DEBUG
  if (block == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  _LargeMemBlock* next_block = block->next_block_;
  _Destroy_LargeMemBlock(block);
  return next_block;
}

int _DestroyAll_LargeMemBlocks(_LargeMemBlock* block) {
#ifdef DEBUG
  if (block == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  if (block->next_block_ != NULL) {
    if (_DestroyAll_LargeMemBlocks(block->next_block_) == ERROR_INVALID_PARAMS) {
      DEBUG_PRINT("Bad params in destructor loop");
    }
  }
  return _Destroy_LargeMemBlock(block);
}

int _DeleteSingle_LargeMemBlock(_LargeMemBlock* root, uint8_t* target) {
#ifdef DEBUG
  if (root == NULL || target == NULL) {
    return ERROR_INVALID_PARAMS;
  }
#endif
  // This does not work for permissions
  //  LargeMemBlock *del_ptr = NULL, **ptr = root;
  //  while (*ptr != NULL && (*ptr)->memory_ + (*ptr)->header_size_ != target) {
  //    ptr = &(*ptr)->next_block_;
  //  }
  //  if (ptr) {
  //    del_ptr              = *ptr;
  //    *ptr                 = del_ptr->next_block_;
  //    del_ptr->next_block_ = NULL;
  //    Destroy_LargeMemBlocks(del_ptr);
  //  }

  _LargeMemBlock *prev, *curr = root;
  uintptr_t       header = root->header_size_;  // We assume root is not null, not for user
  while (curr && curr->memory_ + header) {
    prev = curr;
    curr = curr->next_block_;
  }
  if (curr) {  // Could assume hit, not for now
    _os_protect_readwrite(prev, header);
    prev->next_block_ = curr->next_block_;
    _os_protect_readonly(prev, header);
    _Destroy_LargeMemBlock(curr);
    return SUCCESS;
  } else {
    return ERROR_INVALID_PARAMS;
  }
}

_LargeMemBlock* _Merge_LargeMemBlocks(_LargeMemBlock* first, _LargeMemBlock* second) {
  if (NULL == second) {
    // If both are NULL, return is NULL
    return first;
  }
  if (NULL == first) {
    return second;
  }
  // Do not use the second pointer afterwards!
  _LargeMemBlock* traversed = first;
  while (traversed->next_block_ != NULL) {
    traversed = traversed->next_block_;
  }

  _os_protect_readwrite(traversed, traversed->header_size_);
  traversed->next_block_ = second;
  _os_protect_readonly(traversed, traversed->header_size_);
  return first;
}

#endif