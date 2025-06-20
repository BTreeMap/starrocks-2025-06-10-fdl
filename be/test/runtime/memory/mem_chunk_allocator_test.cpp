#include "runtime/memory/mem_chunk_allocator.h"

#include <gtest/gtest.h>

#include "common/config.h"
#include "runtime/memory/mem_chunk.h"

namespace starrocks {

TEST(MemChunkAllocatorTest, Normal) {
    config::use_mmap_allocate_chunk = true;
    for (size_t size = 4096; size <= 1024 * 1024; size <<= 1) {
        MemChunk chunk;
        ASSERT_TRUE(MemChunkAllocator::instance()->allocate(size, &chunk));
        ASSERT_NE(nullptr, chunk.data);
        ASSERT_EQ(size, chunk.size);
        MemChunkAllocator::instance()->free(chunk);
    }
}
} // namespace starrocks
