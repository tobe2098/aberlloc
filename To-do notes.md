# To do
(Mainly posix, remember all arenas should use a mutex to avoid threading problems).
1. Fixed size buffer with functions like the video (simple arena malloc)
2. Dynamic growth with chaining (chained arena malloc, need special free arena, need linked list and pointer to last. No real cost)
3. Virtual memory mapping extension (virtual mmap malloc) 
- Scratch spaces mmaps
- Combo: 2 and 3 (virtual mmap extends up to a size param, then new block of pages to avoid OOM allocation failure). (chained virtual mmap alloc)
4. Create a vector class that uses 3 & combo (vlarray)
4.5. within arena scratch that leaves space for ret val, (Only in high capacity (either real or virtual))
!!-!!Scratch spaces as reset states of arenas with an offset parameter for the return type alloc. (option only for arenas that are not in limited size blocks/pages, as the scratch space has to be big.).
5. Sub-lifetimes (? Re-watch the video), growing pool allocator using a free-list. Maybe a free-list per byte-length type up to memory page size?
    1. Free list in Red black tree? Or doubly linked list + RB tree
        - Make sure allocations do not spill over to other pages.
6. Memory management depends on objects: Large size is memory page aligned, small size is inside a single page,
medium sized has its own fixed-sized pools.
        - Finding best fit for memory slices.
        - Coalescing memory using backwards and forwards headers every free.
        - Make sure allocations do not spill over to other pages.
    3. Memory management with thread-local pools? Manage cache-line alignment
    4. Bins for different size allocations, cache to keep track of the memory per bin




7. Extend arena with logging, visualization, debugging features that can be enabled.

-Pre: Cheap random number generator that is reliable in C. How much randomness do we need in address randomization?
8. Make an allocator that cannot be exploited: Ensure that the allocation does not raise security flaws in memory discovery (after free), or ASLR due to the page usage. (safeFree()? Zero the data?). Address and allocation randomization, setting memory to zero without compiler optimizing with `volatile`, ensure no under or overflows in memory writing by using guard pages, etc. Look in the conversation: https://chatgpt.com/share/67559d22-ac2c-8009-ba75-75b3c3dcbb0f, https://intmainreturn0.com/notes/secure-allocators.html 
!!=!! For header data I think having the metadata apart, and surrounding each block with canary values is enough, or use header default aligned to 8 bits or more, then give a special function to align at a special alignment.
safebuffer(),
Canary cannot be free bits because I want it to be 32 and future portable and 3-5 bits are not enough to ensure random overflow will not make it happen.
1 random number to use as canary? Problem is, how do I do it in orecompile so it is the same everywhere?
Safe scratch using the pages trick, 

** Small leaves:
- Scratch spaces as mmap calls. Local vs global allocators.



inf. Convert real code
inf+1. Adapt to also use WinAPI.

# Notes
Concepts:
Linear vs monotonic vs multipool?
Natural alignment (char every byte, int every 4, address divisble by size)
Block header (only big blocks), you can get the next block from the size of the current one
Block size +1 if alloc, +0 if not. When getting size, ignore last bits, as address has to be 
in multiples of 8, this is all about using a single integer for metadata, last bits are useless.
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
Essentially, the doubly-linked list is the header, and in free blocks you can make a red-black tree. The problem is dealing with coalescence.
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


Pagesize should be a variable of the Arena

// get the # of bytes currently allocated.
U64 ArenaGetPos(Arena *arena);

// also some useful popping helpers:
void ArenaSetPosBack(Arena *arena, U64 pos);

Pagesize should be a variable of the Arena

For scratch spaces:
struct ArenaTemp
{
  Arena *arena;
  U64 pos;
};

ArenaTemp ArenaTempBegin(Arena *arena); // grabs arena's position
void ArenaTempEnd(ArenaTemp temp);      // restores arena's position
ArenaTemp GetScratch(void); // grabs a thread-local scratch arena
#define ReleaseScratch(t) ArenaTempEnd(t)
Problem: grabbing the same scratch, or writing on top of the same arena you grabbed a scratch from
So, another rule must be adopted. When GetScratch is called, it must take any arenas being used for persistent allocations, to ensure that it returns a different arena, to avoid mixing persistent allocations with scratch allocations. The API, then, becomes the following:

ArenaTemp GetScratch(Arena **conflicts, U64 conflict_count); // grabs a thread-local scratch arena
#define ReleaseScratch(t) ArenaTempEnd(t)
If only a single “persistent” arena is present at any point in any codepath (e.g. a caller never passes in two arenas), then you will not need more than two scratch arenas. Those two scratch arenas can be used for arbitrarily-deep call stacks, because each frame in any call stack will alternate between using a single arena for persistent allocations, and the other for scratch allocations.
“If you want a scratch arena, and an arena is already in scope for persistent allocations, then pass it into GetScratch. If you introduce an arena parameterization into an already-written codepath, then find all instances of GetScratch and update them accordingly”.

For multi-threaded allocators:



static size_t _getPageSize(void) {
#ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
#else
  return sysconf(_SC_PAGESIZE);
#endif
}

static inline size_t align_up(size_t n, size_t align) {
  return (n + align - 1) & ~(align - 1);
}
#include <stdint.h>

uintptr_t align_address(uintptr_t addr, size_t align) {
    if (align == 0) return addr;
    return addr + (align - (addr % align)) % align;
}

// Simplified jemalloc size class computation
size_t size_class = pow(1.25, floor(log(size) / log(1.25)));