#ifndef CACHE_LINE_SIZE

// Apple Silicon
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
#define CACHE_LINE_SIZE 128  // M1/M2/M3

// Windows (using built-in definitions)
#elif defined(_WIN32)
#include <winnt.h>
#define CACHE_LINE_SIZE SYSTEM_CACHE_ALIGNMENT_SIZE

// x86 and x86-64
#elif defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#define CACHE_LINE_SIZE 64

// ARM and ARM64
#elif defined(__arm__)
#if defined(__ARM_ARCH_7A__)
#define CACHE_LINE_SIZE 64  // ARM Cortex-A7, A9, A15
#elif defined(__ARM_ARCH_8A__)
#define CACHE_LINE_SIZE 64  // ARM Cortex-A53, A57, A72
#else
#define CACHE_LINE_SIZE 32  // Older ARM
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
#define CACHE_LINE_SIZE 64  // Generic ARM64

// PowerPC
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#if defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__)
#define CACHE_LINE_SIZE 128  // POWER8, POWER9
#else
#define CACHE_LINE_SIZE 64  // Older PowerPC
#endif

// MIPS
#elif defined(__mips__)
#if defined(__mips64)
#define CACHE_LINE_SIZE 64  // MIPS64
#else
#define CACHE_LINE_SIZE 32  // Older MIPS
#endif

// SPARC
#elif defined(__sparc__)
#if defined(__sparc_v9__)
#define CACHE_LINE_SIZE 64  // SPARC V9
#else
#define CACHE_LINE_SIZE 32  // Older SPARC
#endif

// IBM System Z / s390x
#elif defined(__s390__) || defined(__s390x__)
#define CACHE_LINE_SIZE 256  // z/Architecture

// RISC-V
#elif defined(__riscv)
#if __riscv_xlen == 64
#define CACHE_LINE_SIZE 64  // RV64
#else
#define CACHE_LINE_SIZE 32  // RV32
#endif

// IA64 (Itanium)
#elif defined(__ia64__) || defined(_M_IA64)
#define CACHE_LINE_SIZE 128

// Alpha
#elif defined(__alpha__)
#define CACHE_LINE_SIZE 64

// PA-RISC
#elif defined(__hppa__) || defined(__HPPA__)
#define CACHE_LINE_SIZE 64

// SuperH
#elif defined(__sh__)
#define CACHE_LINE_SIZE 32

// Hexagon
#elif defined(__hexagon__)
#define CACHE_LINE_SIZE 32

// AVR32
#elif defined(__AVR32__)
#define CACHE_LINE_SIZE 32

// Xtensa
#elif defined(__xtensa__)
#define CACHE_LINE_SIZE 32

// NDS32
#elif defined(__nds32__)
#define CACHE_LINE_SIZE 32

// Default fallback
#else
#define CACHE_LINE_SIZE 64
#endif

#endif  // CACHE_LINE_SIZE
