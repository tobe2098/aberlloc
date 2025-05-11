# Mimalloc
## Why mimalloc is Approachable

1. **Clean, Modern Codebase**:
   - Well-documented C implementation
   - Modular design with clear separation of concerns
   - Newer implementation (released in 2019) with clean architecture

2. **Reasonable Complexity**:
   - Simpler than jemalloc and tcmalloc while still being high-performance
   - Core concepts can be implemented in ~2000-3000 lines of code

3. **Available Documentation**:
   - Academic paper explaining the design: "Mimalloc: Free List Sharding in Action"
   - Thorough documentation on GitHub with architecture explanations

4. **Key Concepts That Are Easy to Grasp**:
   - Free list sharding (the core innovation)
   - Page-based memory management
   - Thread-local allocation strategy

## Fundamental Elements to Implement

To capture mimalloc's core strengths, focus on these elements:

1. **Page-based Allocation**:
   - Organize memory in 64KB pages
   - Each page dedicated to a single size class

2. **Free List Sharding**:
   - Separate "local" free lists from "thread-free" lists
   - This is the key innovation that reduces contention

3. **Thread-Local Caching**:
   - Basic thread-local pages for common size classes
   - Simple thread-local free list implementation

4. **Delayed Coalescing**:
   - Hold onto freed memory in thread-local caches
   - Only return to global heap when thread cache is full

These fundamental elements would give you most of mimalloc's performance advantages without implementing all the optimizations like secure mode, huge page support, or segment caches.

## Reasonable Project Scope

A simplified mimalloc could be implemented in stages:

1. Single-threaded allocator with basic size classes
2. Add free list sharding mechanism
3. Add thread-local caching
4. Implement delayed coalescing
5. Add basic large allocation support

This approach would be much more approachable than tackling jemalloc (which has complex tuning parameters) or tcmalloc (which has extensive production optimizations).

Would you like more specific details on any of these implementation aspects?

## Link paper

## Also implement secure mode, huge page support and segment caches. Everything?