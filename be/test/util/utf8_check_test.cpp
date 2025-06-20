#include "util/utf8_check.h"

#include <gtest/gtest.h>

namespace starrocks {

struct test {
    const char* data;
    int len;
};

class Utf8CheckTest : public testing::Test {
public:
    Utf8CheckTest() = default;
    ~Utf8CheckTest() override = default;

private:
    /* positive tests */
    std::vector<test> pos = {{"", 0},
                             {"\x00", 1},
                             {R"(f)", 1},
                             {"\x7F", 1},
                             {"\x00\x7F", 2},
                             {"\x7F\x00", 2},
                             {"\xC2\x80", 2},
                             {"\xDF\xBF", 2},
                             {"\xE0\xA0\x80", 3},
                             {"\xE0\xA0\xBF", 3},
                             {"\xED\x9F\x80", 3},
                             {"\xEF\x80\xBF", 3},
                             {"\xF0\x90\xBF\x80", 4},
                             {"\xF2\x81\xBE\x99", 4},
                             {"\xF4\x8F\x88\xAA", 4}};

    /* negative tests */
    std::vector<test> neg = {{"\x80", 1},
                             {"\xBF", 1},
                             {"\xC0\x80", 2},
                             {"\xC1\x00", 2},
                             {"\xC2\x7F", 2},
                             {"\xDF\xC0", 2},
                             {"\xE0\x9F\x80", 3},
                             {"\xE0\xC2\x80", 3},
                             {"\xED\xA0\x80", 3},
                             {"\xED\x7F\x80", 3},
                             {"\xEF\x80\x00", 3},
                             {"\xF0\x8F\x80\x80", 4},
                             {"\xF0\xEE\x80\x80", 4},
                             {"\xF2\x90\x91\x7F", 4},
                             {"\xF4\x90\x88\xAA", 4},
                             {"\xF4\x00\xBF\xBF", 4},
                             {"\x00\x00\x00\x00\x00\xC2\x80\x00\x00\x00\xE1\x80\x80\x00\x00\xC2"
                              "\xC2\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                              32},
                             {"\x00\x00\x00\x00\x00\xC2\xC2\x80\x00\x00\xE1\x80\x80\x00\x00\x00", 16},
                             {"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                              "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xF1\x80",
                              32},
                             {"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                              "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xF1",
                              32},
                             {"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                              "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xF1\x80"
                              "\x80",
                              33},
                             {"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                              "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xF1\x80"
                              "\xC2\x80",
                              34},
                             {"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                              "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xF0"
                              "\x80\x80\x80",
                              35}};
};
TEST_F(Utf8CheckTest, empty) {
    ASSERT_TRUE(validate_utf8(pos[0].data, pos[0].len));
}

TEST_F(Utf8CheckTest, normal) {
    for (int i = 0; i < sizeof(pos) / sizeof(pos[0]); ++i) {
        ASSERT_TRUE(validate_utf8(pos[i].data, pos[i].len));
    }
}

TEST_F(Utf8CheckTest, abnormal) {
    for (int i = 0; i < sizeof(neg) / sizeof(neg[0]); ++i) {
        ASSERT_FALSE(validate_utf8(neg[i].data, neg[i].len));
    }
}

TEST_F(Utf8CheckTest, naive) {
    for (int i = 0; i < sizeof(pos) / sizeof(pos[0]); ++i) {
        ASSERT_TRUE(validate_utf8_naive(pos[i].data, pos[i].len));
    }
    for (int i = 0; i < sizeof(neg) / sizeof(neg[0]); ++i) {
        std::cout << validate_utf8_naive(neg[i].data, neg[i].len) << std::endl;
        ASSERT_FALSE(validate_utf8_naive(neg[i].data, neg[i].len));
    }
}

} // namespace starrocks
