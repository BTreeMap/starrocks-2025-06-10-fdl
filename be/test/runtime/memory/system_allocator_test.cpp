#include "runtime/memory/system_allocator.h"

#include <gtest/gtest.h>

#include <memory>

#include "common/config.h"
#include "runtime/mem_tracker.h"

namespace starrocks {

template <bool use_mmap>
void test_normal() {
    std::unique_ptr<MemTracker> mem_tracker = std::make_unique<MemTracker>(-1);
    config::use_mmap_allocate_chunk = use_mmap;
    {
        auto ptr = SystemAllocator::allocate(mem_tracker.get(), 4096);
        ASSERT_NE(nullptr, ptr);
        ASSERT_EQ(0, (uint64_t)ptr % 4096);
        SystemAllocator::free(mem_tracker.get(), ptr, 4096);
    }
    {
        auto ptr = SystemAllocator::allocate(mem_tracker.get(), 100);
        ASSERT_NE(nullptr, ptr);
        ASSERT_EQ(0, (uint64_t)ptr % 4096);
        SystemAllocator::free(mem_tracker.get(), ptr, 100);
    }
}

TEST(SystemAllocatorTest, TestNormal) {
    test_normal<true>();
    test_normal<false>();
}

} // namespace starrocks
