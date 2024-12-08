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

