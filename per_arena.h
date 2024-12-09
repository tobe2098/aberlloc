#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <pthread.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#endif

typedef struct Arena {
    uint8_t* __memory;           // Base pointer to reserved memory
    size_t __position;           // Current allocation position
    pthread_mutex_t __arena_mutex;
} Arena;
