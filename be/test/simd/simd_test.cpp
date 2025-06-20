#include "simd/simd.h"

#include "gtest/gtest.h"

namespace starrocks {

class SIMDTest : public testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SIMDTest, count_zeros) {
    EXPECT_EQ(0u, SIMD::count_zero(std::vector<int8_t>{}));
    EXPECT_EQ(3u, SIMD::count_zero(std::vector<int8_t>{0, 0, 0}));
    EXPECT_EQ(1u, SIMD::count_zero(std::vector<int8_t>{0, 1, 2}));
    EXPECT_EQ(1u, SIMD::count_zero(std::vector<int8_t>{-1, 0, 1}));

    // size greater than 64 will use SSE2 instructions.
    std::vector<int8_t> nums(100, 0);
    EXPECT_EQ(100u, SIMD::count_zero(nums));
    nums.emplace_back(1);
    EXPECT_EQ(100u, SIMD::count_zero(nums));
    nums.emplace_back(0);
    EXPECT_EQ(101u, SIMD::count_zero(nums));
    nums.emplace_back(-1);
    EXPECT_EQ(101u, SIMD::count_zero(nums));
}

TEST_F(SIMDTest, count_nonzero) {
    // size less than 64, count by loop.
    EXPECT_EQ(0u, SIMD::count_nonzero(std::vector<int8_t>{}));
    EXPECT_EQ(0u, SIMD::count_nonzero(std::vector<int8_t>{0, 0, 0}));
    EXPECT_EQ(3u, SIMD::count_nonzero(std::vector<int8_t>{1, 1, 1}));
    EXPECT_EQ(3u, SIMD::count_nonzero(std::vector<int8_t>{-1, 1, 2}));
    EXPECT_EQ(2u, SIMD::count_nonzero(std::vector<int8_t>{0, 1, 2}));

    // size greater than 64 will use SSE2 instructions.
    std::vector<uint8_t> numbers(100, 0);
    EXPECT_EQ(0u, SIMD::count_nonzero(numbers));

    for (int i = 1; i <= 10; i++) {
        numbers.emplace_back(1);
    }
    EXPECT_EQ(10u, SIMD::count_nonzero(numbers));

    for (int i = 1; i <= 10; i++) {
        numbers.emplace_back(i);
    }
    EXPECT_EQ(20u, SIMD::count_nonzero(numbers));

    for (int i = 1; i <= 10; i++) {
        numbers.emplace_back(0 - i);
    }
    EXPECT_EQ(30u, SIMD::count_nonzero(numbers));
}

TEST_F(SIMDTest, count_zeros_int32) {
    EXPECT_EQ(0u, SIMD::count_zero(std::vector<uint32_t>{}));
    EXPECT_EQ(3u, SIMD::count_zero(std::vector<uint32_t>{0, 0, 0}));
    EXPECT_EQ(1u, SIMD::count_zero(std::vector<uint32_t>{0, 1, 2}));
    EXPECT_EQ(1u, SIMD::count_zero(std::vector<uint32_t>{11, 0, 1}));

    // size greater than 64 will use SSE2 instructions.
    std::vector<uint32_t> nums(100, 0);
    EXPECT_EQ(100u, SIMD::count_zero(nums));
    nums.emplace_back(1);
    EXPECT_EQ(100u, SIMD::count_zero(nums));
    nums.emplace_back(0);
    EXPECT_EQ(101u, SIMD::count_zero(nums));
    nums.emplace_back(11);
    EXPECT_EQ(101u, SIMD::count_zero(nums));
}

TEST_F(SIMDTest, count_nonzero_int32) {
    // size less than 64, count by loop.
    EXPECT_EQ(0u, SIMD::count_nonzero(std::vector<uint32_t>{}));
    EXPECT_EQ(0u, SIMD::count_nonzero(std::vector<uint32_t>{0, 0, 0}));
    EXPECT_EQ(3u, SIMD::count_nonzero(std::vector<uint32_t>{1, 1, 1}));
    EXPECT_EQ(3u, SIMD::count_nonzero(std::vector<uint32_t>{11, 1, 2}));
    EXPECT_EQ(2u, SIMD::count_nonzero(std::vector<uint32_t>{0, 1, 2}));

    // size greater than 64 will use SSE2 instructions.
    std::vector<uint32_t> numbers(100, 0);
    EXPECT_EQ(0u, SIMD::count_nonzero(numbers));

    for (int i = 1; i <= 10; i++) {
        numbers.emplace_back(1);
    }
    EXPECT_EQ(10u, SIMD::count_nonzero(numbers));

    for (int i = 1; i <= 10; i++) {
        numbers.emplace_back(i);
    }
    EXPECT_EQ(20u, SIMD::count_nonzero(numbers));

    for (int i = 1; i <= 10; i++) {
        numbers.emplace_back(i + 100);
    }
    EXPECT_EQ(30u, SIMD::count_nonzero(numbers));
}

TEST_F(SIMDTest, contains_nonzero_bit) {
    std::vector<uint8_t> nums;
    for (int i = 0; i < 1000; i++) {
        nums.emplace_back(0);
    }
    EXPECT_FALSE(SIMD::contains_nonzero_bit(nums.data(), nums.size()));

    // non-zero in non-SIMD check tail part.
    nums.emplace_back(8);
    EXPECT_TRUE(SIMD::contains_nonzero_bit(nums.data(), nums.size()));

    // non-zero in SIMD check part.
    for (int i = 0; i < 1000; i++) {
        nums.emplace_back(0);
    }
    EXPECT_TRUE(SIMD::contains_nonzero_bit(nums.data(), nums.size()));
}

} // namespace starrocks
