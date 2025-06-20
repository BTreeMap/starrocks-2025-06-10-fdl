#include "util/bit_util.h"

#include <gtest/gtest.h>

#include <boost/utility.hpp>

namespace starrocks {

TEST(BitUtil, Ceil) {
    EXPECT_EQ(BitUtil::ceil(0, 1), 0);
    EXPECT_EQ(BitUtil::ceil(1, 1), 1);
    EXPECT_EQ(BitUtil::ceil(1, 2), 1);
    EXPECT_EQ(BitUtil::ceil(1, 8), 1);
    EXPECT_EQ(BitUtil::ceil(7, 8), 1);
    EXPECT_EQ(BitUtil::ceil(8, 8), 1);
    EXPECT_EQ(BitUtil::ceil(9, 8), 2);
}

TEST(BitUtil, Popcount) {
    EXPECT_EQ(BitUtil::popcount(BOOST_BINARY(0 1 0 1 0 1 0 1)), 4);
    EXPECT_EQ(BitUtil::popcount_no_hw(BOOST_BINARY(0 1 0 1 0 1 0 1)), 4);
    EXPECT_EQ(BitUtil::popcount(BOOST_BINARY(1 1 1 1 0 1 0 1)), 6);
    EXPECT_EQ(BitUtil::popcount_no_hw(BOOST_BINARY(1 1 1 1 0 1 0 1)), 6);
    EXPECT_EQ(BitUtil::popcount(BOOST_BINARY(1 1 1 1 1 1 1 1)), 8);
    EXPECT_EQ(BitUtil::popcount_no_hw(BOOST_BINARY(1 1 1 1 1 1 1 1)), 8);
    EXPECT_EQ(BitUtil::popcount(0), 0);
    EXPECT_EQ(BitUtil::popcount_no_hw(0), 0);
}

TEST(BitUtil, RoundUp) {
    EXPECT_EQ(BitUtil::round_up_numi32(1), 1);
    EXPECT_EQ(BitUtil::round_up_numi32(0), 0);
}

TEST(BitUtil, RoundDown) {
    EXPECT_EQ(BitUtil::RoundDownToPowerOf2(7, 4), 4);
}

} // namespace starrocks
