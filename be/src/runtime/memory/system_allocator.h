#pragma once

#include <cstddef>
#include <cstdint>

namespace starrocks {

class MemTracker;

// Allocate memory from system allocator, this allocator can be configured
// to allocate memory via mmap or malloc.
class SystemAllocator {
public:
    static uint8_t* allocate(MemTracker* mem_tracker, size_t length);

    static void free(MemTracker* mem_tracker, uint8_t* ptr, size_t length);

private:
    static uint8_t* allocate_via_mmap(MemTracker* mem_tracker, size_t length);
    static uint8_t* allocate_via_malloc(size_t length);
};

} // namespace starrocks
