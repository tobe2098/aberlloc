#include <stdint.h>

#ifdef _WIN32
#ifdef __GNUC__
#include <pthread.h>
#include <windows.h>
// Compilation using msys2 env or similar
#else
#error "You need to compile with gcc."
#endif
#else
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
// Fixed size arena
typedef struct SimpleArena {
    uint8_t*        __memory;      // Base pointer to reserved memory
    size_t          __position;    // Current allocation position
    size_t          __total_size;  // Size
    pthread_mutex_t __arena_mutex;
    int             __auto_align;
} SimpleArena;

int Alloc_SimpleArena(SimpleArena& arena, int no_pages) { }
int Release_SimpleArena(SimpleArena& arena) { }

int SetAutoAlign_SimpleArena(SimpleArena& arena, int alignment) { }
int GetPos_SimpleArena(SimpleArena* arena) { }

int PushAligner_SimpleArena(SimpleArena* arena, int alignemnt) { }
int PushNoZero_SimpleArena(SimpleArena* arena, int bytes) { }
int Push_SimpleArena(SimpleArena* arena, int bytes) { }

int PopTo_SimpleArena(SimpleArena* arena, int position) { }
int Pop_SimpleArena(SimpleArena* arena, int bytes) { }
int Clear_SimpleArena(SimpleArena* arena) { }

// ArenaAlloc: Creates new arena with reserved virtual memory
// ArenaRelease: Releases all arena memory
// ArenaSetAutoAlign: Controls automatic alignment behavior
// ArenaPos: Returns current allocation position
// ArenaPushNoZero: Allocates uninitialized memory
// ArenaPushAligner: Adds alignment padding
// ArenaPush: Allocates zero-initialized memory
// ArenaPopTo: Returns to specific position
// ArenaPop: Removes specified amount from top
// ArenaClear: Resets arena to empty state