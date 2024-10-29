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
5.1 Free list in Red black tree
5.5 Memory management depends on objects: Large size is memory page aligned, small size is inside a single page,
medium sized has its own fixed-sized pools.
5.58 Make sure allocations do not spill over to other pages
5.55 Scratch spaces as mmap calls. Local vs global allocators
5.56 Finding best fit for memory slices.
5.57 Coalescing memory using backwards and forwards headers every free.
5.6 Memory management with thread-local pools? Manage cache-line alignment
5.7 Bins for different size allocations, cache to keep track of the memory per bin


5.8 Ensure that the allocation does not raise security flaws in memory discovery (after free),
or ASLR due to the page usage. 
// Simplified jemalloc size class computation
size_t size_class = pow(1.25, floor(log(size) / log(1.25)));

6. Extend arena with logging, visualization, debugging features that can be enabled.
7.Convert real code
Concepts:
Linear vs monotonic vs multipool?
Natural alignment (char every byte, int every 4, address divisble by size)
Block header (only big blocks), you can get the next block from the size of the current one
Block size +1 if alloc, +0 if not. When getting size, ignore last bits, as address has to be 
in multiples of 8
Size 0 is end of chunk
Header plus footer makes it possible to iterate backwards.
- Using Red Black trees we can reduce its complexity to O(log N) while keeping 
space complexity quite low because the tree data is stored inside the free
memory blocks. In addition, this structure allows a best-fit algorithm to be
used, reducing the fragmentation and keeping performance. However, an
additional sorted Doubly Linked list is required to store allocated and 
free elements in order to be able to do coalescence operations in O(1).
This implementation is the most common and most used in real systems 
because it offers high flexibility while keeping performance very high.
- 
Funcs:
ArenaAlloc: Creates new arena with reserved virtual memory
ArenaRelease: Releases all arena memory
ArenaSetAutoAlign: Controls automatic alignment behavior
ArenaPos: Returns current allocation position
ArenaPushNoZero: Allocates uninitialized memory
ArenaPushAligner: Adds alignment padding
ArenaPush: Allocates zero-initialized memory
ArenaPopTo: Returns to specific position
ArenaPop: Removes specified amount from top
ArenaClear: Resets arena to empty state


Pagesize should be a variable of the Arena*/