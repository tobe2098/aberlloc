typedef struct MemoryBlock {
    size_t offset;      // Offset from arena base
    size_t size;        // Size of this block
    struct MemoryBlock* next;
} MemoryBlock;

typedef struct Arena {
    uint8_t* memory;           // Base pointer to reserved memory
    size_t reserved_size;      // Total reserved size
    size_t committed_size;     // Total committed size
    size_t position;           // Current allocation position
    size_t alignment;          // Default alignment
    bool auto_align;           // Auto alignment flag
    MemoryBlock* blocks;       // List of committed memory blocks
    MemoryBlock* free_blocks;  // Free block pool for reuse
} Arena;

// Helper to find gaps between committed regions
static void find_best_fit(Arena* arena, size_t size, size_t* best_offset, size_t* best_size) {
    size_t page_size = get_page_size();
    size = align_up(size, page_size);
    
    *best_offset = (size_t)-1;
    *best_size = (size_t)-1;
    
    // Start with checking from current position
    size_t current_offset = align_up(arena->position, page_size);
    MemoryBlock* block = arena->blocks;
    
    while (block) {
        // Check gap before this block
        if (current_offset < block->offset) {
            size_t gap = block->offset - current_offset;
            if (gap >= size && gap < *best_size) {
                *best_offset = current_offset;
                *best_size = gap;
            }
        }
        current_offset = block->offset + block->size;
        block = block->next;
    }
    
    // Check gap after last block
    if (current_offset < arena->reserved_size) {
        size_t gap = arena->reserved_size - current_offset;
        if (gap >= size && gap < *best_size) {
            *best_offset = current_offset;
            *best_size = gap;
        }
    }
}

static bool commit_memory_block(Arena* arena, size_t required_size) {
    size_t page_size = get_page_size();
    size_t aligned_size = align_up(required_size, page_size);
    
    // Try to find best fitting gap
    size_t best_offset, best_size;
    find_best_fit(arena, aligned_size, &best_offset, &best_size);
    
    if (best_offset == (size_t)-1) {
        // No suitable gap found
        return false;
    }
    
    // Try to commit the memory
    if (!os_commit(arena->memory + best_offset, aligned_size)) {
        return false;
    }
    
    // Allocate new block descriptor
    MemoryBlock* new_block;
    if (arena->free_blocks) {
        new_block = arena->free_blocks;
        arena->free_blocks = new_block->next;
    } else {
        new_block = malloc(sizeof(MemoryBlock));
        if (!new_block) {
            os_decommit(arena->memory + best_offset, aligned_size);
            return false;
        }
    }
    
    // Initialize new block
    new_block->offset = best_offset;
    new_block->size = aligned_size;
    
    // Insert into sorted list
    MemoryBlock** ptr = &arena->blocks;
    while (*ptr && (*ptr)->offset < best_offset) {
        ptr = &(*ptr)->next;
    }
    new_block->next = *ptr;
    *ptr = new_block;
    
    arena->committed_size += aligned_size;
    return true;
}

static void coalesce_blocks(Arena* arena) {
    MemoryBlock* block = arena->blocks;
    while (block && block->next) {
        if (block->offset + block->size == block->next->offset) {
            // Merge blocks
            MemoryBlock* to_remove = block->next;
            block->size += to_remove->size;
            block->next = to_remove->next;
            
            // Add to free block pool
            to_remove->next = arena->free_blocks;
            arena->free_blocks = to_remove;
        } else {
            block = block->next;
        }
    }
}

static bool ensure_committed(Arena* arena, size_t required_size) {
    if (!arena || required_size > arena->reserved_size) {
        return false;
    }
    
    // Find block containing current position
    MemoryBlock* current_block = NULL;
    size_t remaining = 0;
    
    for (MemoryBlock* block = arena->blocks; block; block = block->next) {
        if (arena->position >= block->offset && 
            arena->position < block->offset + block->size) {
            current_block = block;
            remaining = block->offset + block->size - arena->position;
            break;
        }
    }
    
    if (!current_block || remaining < required_size) {
        // Need to commit new block
        if (!commit_memory_block(arena, required_size)) {
            return false;
        }
        coalesce_blocks(arena);
    }
    
    return true;
}

void ArenaRelease(Arena* arena) {
    if (!arena) return;
    
    // Free all blocks and block pool
    while (arena->blocks) {
        MemoryBlock* next = arena->blocks->next;
        free(arena->blocks);
        arena->blocks = next;
    }
    while (arena->free_blocks) {
        MemoryBlock* next = arena->free_blocks->next;
        free(arena->free_blocks);
        arena->free_blocks = next;
    }
    
    if (arena->memory) {
        os_release(arena->memory, arena->reserved_size);
    }
    free(arena);
}

void ArenaPopTo(Arena* arena, size_t pos) {
    if (!arena || pos > arena->position) return;
    
    size_t page_size = get_page_size();
    size_t new_pos_pages = align_up(pos, page_size);
    
    // Decommit blocks that are entirely after new position
    MemoryBlock** ptr = &arena->blocks;
    while (*ptr) {
        MemoryBlock* block = *ptr;
        if (block->offset >= new_pos_pages) {
            // Decommit and remove block
            os_decommit(arena->memory + block->offset, block->size);
            arena->committed_size -= block->size;
            *ptr = block->next;
            
            // Add to free block pool
            block->next = arena->free_blocks;
            arena->free_blocks = block;
        } else {
            ptr = &block->next;
        }
    }
    
    arena->position = pos;
}
typedef enum ArenaError {
    ARENA_OK,
    ARENA_ERROR_VIRTUAL_EXHAUSTED,
    ARENA_ERROR_PHYSICAL_EXHAUSTED,
    ARENA_ERROR_PARTIAL_COMMIT
} ArenaError;

typedef struct Arena {
    uint8_t* memory;
    size_t reserved_size;
    size_t committed_size;
    size_t position;
    size_t alignment;
    bool auto_align;
    ArenaError last_error;  // Track last error
    size_t failed_commit_size;  // Size of failed commit attempt
} Arena;

static ArenaError ensure_committed_ex(Arena* arena, size_t required_size, size_t* committed) {
    if (!arena) return ARENA_ERROR_VIRTUAL_EXHAUSTED;
    
    size_t page_size = get_page_size();
    size_t new_committed_size = align_up(required_size, page_size);
    
    if (new_committed_size > arena->reserved_size) {
        arena->failed_commit_size = new_committed_size - arena->reserved_size;
        return ARENA_ERROR_VIRTUAL_EXHAUSTED;
    }
    
    if (new_committed_size > arena->committed_size) {
        uint8_t* commit_start = arena->memory + arena->committed_size;
        size_t commit_size = new_committed_size - arena->committed_size;
        
        // Try full commit
        if (!os_commit(commit_start, commit_size)) {
            // Try partial commit
            size_t partial_size = page_size;
            while (partial_size < commit_size) {
                if (os_commit(commit_start, partial_size)) {
                    *committed = partial_size;
                    arena->committed_size += partial_size;
                    arena->failed_commit_size = commit_size - partial_size;
                    return ARENA_ERROR_PARTIAL_COMMIT;
                }
                partial_size += page_size;
            }
            arena->failed_commit_size = commit_size;
            return ARENA_ERROR_PHYSICAL_EXHAUSTED;
        }
        
        arena->committed_size = new_committed_size;
        *committed = commit_size;
    }
    
    return ARENA_OK;
}

void* ArenaPushNoZero(Arena* arena, size_t size) {
    if (!arena || size == 0) return NULL;
    
    size_t aligned_pos = arena->position;
    if (arena->auto_align) {
        aligned_pos = align_up(aligned_pos, arena->alignment);
    }
    
    size_t new_pos = aligned_pos + size;
    size_t committed = 0;
    
    ArenaError error = ensure_committed_ex(arena, new_pos, &committed);
    if (error != ARENA_OK) {
        arena->last_error = error;
        
        // Handle partial commit case
        if (error == ARENA_ERROR_PARTIAL_COMMIT) {
            // We got some memory, but not all we wanted
            size_t available = committed - (aligned_pos - arena->committed_size);
            if (available > 0) {
                // Return partial allocation if possible
                arena->position = aligned_pos + available;
                return arena->memory + aligned_pos;
            }
        }
        return NULL;
    }
    
    void* result = arena->memory + aligned_pos;
    arena->position = new_pos;
    return result;
}

// Add function to query arena status
typedef struct ArenaStatus {
    size_t reserved_size;
    size_t committed_size;
    size_t used_size;
    size_t last_failed_commit;
    ArenaError last_error;
    bool is_fragmented;
} ArenaStatus;

void ArenaGetStatus(Arena* arena, ArenaStatus* status) {
    if (!arena || !status) return;
    
    status->reserved_size = arena->reserved_size;
    status->committed_size = arena->committed_size;
    status->used_size = arena->position;
    status->last_failed_commit = arena->failed_commit_size;
    status->last_error = arena->last_error;
    status->is_fragmented = arena->position < arena->committed_size;
}
