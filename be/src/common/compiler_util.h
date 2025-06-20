#pragma once

// Compiler hint that this branch is likely or unlikely to
// be taken. Take from the "What all programmers should know
// about memory" paper.
// example: if (LIKELY(size > 0)) { ... }
// example: if (UNLIKELY(!status.ok())) { ... }
#define CACHE_LINE_SIZE 64

#ifdef LIKELY
#undef LIKELY
#endif

#ifdef UNLIKELY
#undef UNLIKELY
#endif

#define LIKELY(expr) __builtin_expect(!!(expr), 1)
#define UNLIKELY(expr) __builtin_expect(!!(expr), 0)

#define PREFETCH(addr) __builtin_prefetch(addr)

/// Force inlining. The 'inline' keyword is treated by most compilers as a hint,
/// not a command. This should be used sparingly for cases when either the function
/// needs to be inlined for a specific reason or the compiler's heuristics make a bad
/// decision, e.g. not inlining a small function on a hot path.
#define ALWAYS_INLINE __attribute__((always_inline))
#define ALWAYS_NOINLINE __attribute__((noinline))

#define ALIGN_CACHE_LINE __attribute__((aligned(CACHE_LINE_SIZE)))

#ifndef DIAGNOSTIC_PUSH
#ifdef __clang__
#define DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")
#define DIAGNOSTIC_POP _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")
#define DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define DIAGNOSTIC_PUSH __pragma(warning(push))
#define DIAGNOSTIC_POP __pragma(warning(pop))
#else
#error("Unknown compiler")
#endif
#endif // ifndef DIAGNOSTIC_PUSH

#ifndef DIAGNOSTIC_IGNORE
#define PRAGMA(TXT) _Pragma(#TXT)
#ifdef __clang__
#define DIAGNOSTIC_IGNORE(XXX) PRAGMA(clang diagnostic ignored XXX)
#elif defined(__GNUC__)
#define DIAGNOSTIC_IGNORE(XXX) PRAGMA(GCC diagnostic ignored XXX)
#elif defined(_MSC_VER)
#define DIAGNOSTIC_IGNORE(XXX) __pragma(warning(disable : XXX))
#else
#define DIAGNOSTIC_IGNORE(XXX)
#endif
#endif // ifndef DIAGNOSTIC_IGNORE
