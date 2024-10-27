#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

typedef struct Arena {
    uint8_t* memory;           // Base pointer to reserved memory
    size_t reserved_size;      // Total reserved size
    size_t committed_size;     // Currently committed size
    size_t position;           // Current allocation position
    size_t alignment;          // Default alignment for allocations
    bool auto_align;           // Whether to automatically align allocations
} Arena;

// Platform-specific functions for virtual memory management
static void* os_reserve(size_t size) {
#ifdef _WIN32
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
#else
    void* ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (ptr == MAP_FAILED) ? NULL : ptr;
#endif
}

static bool os_commit(void* ptr, size_t size) {
#ifdef _WIN32
    return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
#else
    return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0;
#endif
}

static bool os_decommit(void* ptr, size_t size) {
#ifdef _WIN32
    return VirtualFree(ptr, size, MEM_DECOMMIT);
#else
    return mprotect(ptr, size, PROT_NONE) == 0;
#endif
}

static bool os_release(void* ptr, size_t size) {
#ifdef _WIN32
    return VirtualFree(ptr, 0, MEM_RELEASE);
#else
    return munmap(ptr, size) == 0;
#endif
}

// Helper function to align size up to the next multiple of alignment
static size_t align_up(size_t size, size_t alignment) {
    return (size + (alignment - 1)) & ~(alignment - 1);
}

// Get system page size
static size_t get_page_size(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    return sysconf(_SC_PAGESIZE);
#endif
}

// Initialize an arena with reserved virtual memory
Arena* ArenaAlloc(size_t reserve_size) {
    // Round up to page size
    size_t page_size = get_page_size();
    reserve_size = align_up(reserve_size, page_size);
    
    // Allocate arena structure (committed memory)
    Arena* arena = malloc(sizeof(Arena));
    if (!arena) return NULL;
    
    // Reserve virtual memory
    void* memory = os_reserve(reserve_size);
    if (!memory) {
        free(arena);
        return NULL;
    }
    
    // Initialize arena
    arena->memory = memory;
    arena->reserved_size = reserve_size;
    arena->committed_size = 0;
    arena->position = 0;
    arena->alignment = sizeof(void*);  // Default alignment
    arena->auto_align = true;
    
    return arena;
}

// Release all arena memory
void ArenaRelease(Arena* arena) {
    if (!arena) return;
    if (arena->memory) {
        os_release(arena->memory, arena->reserved_size);
    }
    free(arena);
}

// Set automatic alignment
void ArenaSetAutoAlign(Arena* arena, bool auto_align) {
    if (!arena) return;
    arena->auto_align = auto_align;
}

// Get current position in arena
size_t ArenaPos(Arena* arena) {
    return arena ? arena->position : 0;
}

// Ensure enough committed memory is available
static bool ensure_committed(Arena* arena, size_t required_size) {
    if (!arena) return false;
    
    size_t page_size = get_page_size();
    size_t new_committed_size = align_up(required_size, page_size);
    
    if (new_committed_size > arena->reserved_size) {
        return false;
    }
    
    if (new_committed_size > arena->committed_size) {
        uint8_t* commit_start = arena->memory + arena->committed_size;
        size_t commit_size = new_committed_size - arena->committed_size;
        
        if (!os_commit(commit_start, commit_size)) {
            return false;
        }
        
        arena->committed_size = new_committed_size;
    }
    
    return true;
}

// Push memory without zeroing
void* ArenaPushNoZero(Arena* arena, size_t size) {
    if (!arena || size == 0) return NULL;
    
    size_t aligned_pos = arena->position;
    if (arena->auto_align) {
        aligned_pos = align_up(aligned_pos, arena->alignment);
    }
    
    size_t new_pos = aligned_pos + size;
    if (!ensure_committed(arena, new_pos)) {
        return NULL;
    }
    
    void* result = arena->memory + aligned_pos;
    arena->position = new_pos;
    return result;
}

// Push alignment padding
void* ArenaPushAligner(Arena* arena, size_t alignment) {
    if (!arena) return NULL;
    
    size_t current = arena->position;
    size_t aligned = align_up(current, alignment);
    size_t padding = aligned - current;
    
    if (padding > 0) {
        return ArenaPushNoZero(arena, padding);
    }
    
    return NULL;
}

// Push zeroed memory
void* ArenaPush(Arena* arena, size_t size) {
    void* result = ArenaPushNoZero(arena, size);
    if (result) {
        memset(result, 0, size);
    }
    return result;
}

// Pop arena to specific position
void ArenaPopTo(Arena* arena, size_t pos) {
    if (!arena || pos > arena->position) return;
    
    size_t page_size = get_page_size();
    size_t old_committed_pages = arena->committed_size / page_size;
    size_t new_pos_pages = align_up(pos, page_size) / page_size;
    
    if (new_pos_pages < old_committed_pages) {
        size_t decommit_start = new_pos_pages * page_size;
        size_t decommit_size = arena->committed_size - decommit_start;
        
        if (os_decommit(arena->memory + decommit_start, decommit_size)) {
            arena->committed_size = decommit_start;
        }
    }
    
    arena->position = pos;
}

// Pop specified size from arena
void ArenaPop(Arena* arena, size_t size) {
    if (!arena || size > arena->position) return;
    ArenaPopTo(arena, arena->position - size);
}

// Clear arena
void ArenaClear(Arena* arena) {
    if (!arena) return;
    ArenaPopTo(arena, 0);
}
