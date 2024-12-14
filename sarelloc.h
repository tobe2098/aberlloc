#include <stdint.h>

#ifdef _WIN32
#ifdef __GNUC__
#include <windows.h>
#include <pthread.h>
//Compilation using msys2 env or similar
#else
#error "You need to compile with gcc"
#endif
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

init(int no_pages);