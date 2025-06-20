#pragma once

#include <cstddef>
#include <cstdint>

namespace starrocks {

// A chunk of continuous memory.
// Almost all files depend on this struct, and each modification
// will result in recompilation of all files. So, we put it in a
// file to keep this file simple and infrequently changed.
struct MemChunk {
    uint8_t* data = nullptr;
    size_t size;
    int core_id;
};

} // namespace starrocks
