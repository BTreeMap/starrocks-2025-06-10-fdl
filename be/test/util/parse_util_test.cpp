#include "util/parse_util.h"

#include <gtest/gtest.h>

#include <string>

#include "testutil/assert.h"

namespace starrocks {

const static int64_t test_memory_limit = 10000;

static void test_parse_mem_spec(const std::string& mem_spec_str, int64_t result) {
    ASSIGN_OR_ASSERT_FAIL(int64_t bytes, ParseUtil::parse_mem_spec(mem_spec_str, test_memory_limit));
    ASSERT_EQ(result, bytes);
}

TEST(TestParseMemSpec, Normal) {
    test_parse_mem_spec("1", 1);

    test_parse_mem_spec("100b", 100);
    test_parse_mem_spec("100B", 100);

    test_parse_mem_spec("5k", 5 * 1024L);
    test_parse_mem_spec("512K", 512 * 1024L);

    test_parse_mem_spec("4m", 4 * 1024 * 1024L);
    test_parse_mem_spec("46M", 46 * 1024 * 1024L);

    test_parse_mem_spec("8g", 8 * 1024 * 1024 * 1024L);
    test_parse_mem_spec("128G", 128 * 1024 * 1024 * 1024L);

    test_parse_mem_spec("8t", 8L * 1024 * 1024 * 1024 * 1024L);
    test_parse_mem_spec("128T", 128L * 1024 * 1024 * 1024 * 1024L);

    test_parse_mem_spec("20%", test_memory_limit * 0.2);
    test_parse_mem_spec("-1", -1);
    test_parse_mem_spec("", 0);
}

TEST(TestParseMemSpec, Bad) {
    std::vector<std::string> bad_values{{"1gib"}, {"1%b"}, {"1b%"}, {"gb"},  {"1GMb"}, {"1b1Mb"}, {"1kib"},
                                        {"1Bb"},  {"1%%"}, {"1.1"}, {"1pb"}, {"1eb"},  {"%"}};
    for (const auto& value : bad_values) {
        ASSERT_ERROR(ParseUtil::parse_mem_spec(value, test_memory_limit));
    }
}

} // namespace starrocks
