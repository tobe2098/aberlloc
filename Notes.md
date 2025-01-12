# Static arena
- Allocation avoids malloc (to ensure usage without library).
- To ensure memory is not overwritten from the base arena, the mutex from the arena is permanently locked until the scratch space is freed. NO, this approach will ensure that bad usage ends up in deadlocks, even if in threading operations, if scratch arenas are created, they are inherently protected from data racing until destruction, this should be handled by the user.
- To ensure corruption of scratch spaces does not result in deadlocking or similar behaviour, the mutexes are always allocated in spaces of memory out of reach from the pointers (the stack). The headers for arenas should never be allocated in the arenas they allocate their own block from.

# Virtual arena
- Physical pages are not contiguous, even if the OS fools you with contiguous virtual memory adress spaces.
- Input based checks can be performed outside the lock. Internal state checks must always be performed inside the lock to avoid data racing.
- Scratch space is a static arena, as the virtual pointer ownership is of the parent.
# General notes
- On both arenas, the mutexing makes no sense as they are supposed to be used in a single thread.