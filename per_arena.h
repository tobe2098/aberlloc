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
    size_t position;           // Current allocation position
} Arena;

/*
1. Fixed size buffer with functions like the video
2. Dynamic growth with chaining
3. Virtual memory mapping (enable both 2 and 3 dynamically too)
4. Create a vector class that uses 3.
5. Sub-lifetimes, growing pool allocator using a free-list. Maybe a free-list per byte-length type up to memory page size?
6. Extend arena with logging, visualization, debugging features that can be enabled.
7.Convert real code*/