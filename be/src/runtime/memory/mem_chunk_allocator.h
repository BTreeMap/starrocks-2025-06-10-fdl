#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace starrocks {

struct MemChunk;
class ChunkArena;
class MemTracker;

// Used to allocate memory with power-of-two length.
// This Allocator allocate memory from system and cache free chunks for
// later use.
//
// MemChunkAllocator has one ChunkArena for each CPU core, it will try to allocate
// memory from current core arena firstly. In this way, there will be no lock contention
// between concurrently-running threads. If this fails, MemChunkAllocator will try to allocate
// memory from other core's arena.
//
// Memory Reservation
// MemChunkAllocator has a limit about how much free chunk bytes it can reserve, above which
// chunk will be released to system memory. For the worst case, when the limits is 0, it will
// act as allocating directly from system.
//
// ChunkArena will keep a separate free list for each chunk size. In common case, chunk will
// be allocated from current core arena. In this case, there is no lock contention.
//
// Must call CpuInfo::init() and StarRocksMetrics::instance()->initialize() to achieve good performance
// before first object is created. And call init_instance() before use instance is called.
class MemChunkAllocator {
public:
    static void init_instance(MemTracker* mem_tracker, size_t reserve_limit);

#ifdef BE_TEST
    static MemChunkAllocator* instance();
#else
    static MemChunkAllocator* instance() { return _s_instance; }
#endif

    MemChunkAllocator(MemTracker* mem_tracker, size_t reserve_limit);

    // Allocate a MemChunk with a power-of-two length "size".
    // Return true if success and allocated chunk is saved in "chunk".
    // Otherwise return false.
    bool allocate(size_t size, MemChunk* chunk);

    // Free chunk allocated from this allocator
    void free(const MemChunk& chunk);

    void set_mem_tracker(MemTracker* mem_tracker) { _mem_tracker = mem_tracker; }

private:
    static MemChunkAllocator* _s_instance;

    MemTracker* _mem_tracker = nullptr;
    size_t _reserve_bytes_limit;
    std::atomic<int64_t> _reserved_bytes;
    // each core has a ChunkArena
    std::vector<std::unique_ptr<ChunkArena>> _arenas;
};

} // namespace starrocks
