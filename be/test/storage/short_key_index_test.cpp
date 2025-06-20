#include "storage/short_key_index.h"

#include <gtest/gtest.h>

namespace starrocks {

class ShortKeyIndexTest : public testing::Test {
public:
    ShortKeyIndexTest() = default;
    ~ShortKeyIndexTest() override = default;
};

TEST_F(ShortKeyIndexTest, buider) {
    ShortKeyIndexBuilder builder(0, 1024);

    int num_items = 0;
    for (int i = 1000; i < 10000; i += 2) {
        builder.add_item(std::to_string(i));
        num_items++;
    }
    std::vector<Slice> slices;
    PageFooterPB footer;
    auto st = builder.finalize(9000 * 1024, &slices, &footer);
    ASSERT_TRUE(st.ok());
    ASSERT_EQ(SHORT_KEY_PAGE, footer.type());
    ASSERT_EQ(num_items, footer.short_key_page_footer().num_items());

    std::string buf;
    for (auto& slice : slices) {
        buf.append(slice.data, slice.size);
    }

    ShortKeyIndexDecoder decoder;
    st = decoder.parse(buf, footer.short_key_page_footer());
    ASSERT_TRUE(st.ok());

    // find 1499
    {
        auto iter = decoder.lower_bound("1499");
        ASSERT_TRUE(iter.valid());
        ASSERT_STREQ("1500", (*iter).to_string().c_str());
    }
    // find 1500 lower bound
    {
        auto iter = decoder.lower_bound("1500");
        ASSERT_TRUE(iter.valid());
        ASSERT_STREQ("1500", (*iter).to_string().c_str());
    }
    // find 1500 upper bound
    {
        auto iter = decoder.upper_bound("1500");
        ASSERT_TRUE(iter.valid());
        ASSERT_STREQ("1502", (*iter).to_string().c_str());
    }
    // find prefix "87"
    {
        auto iter = decoder.lower_bound("87");
        ASSERT_TRUE(iter.valid());
        ASSERT_STREQ("8700", (*iter).to_string().c_str());
    }
    // find prefix "87"
    {
        auto iter = decoder.upper_bound("87");
        ASSERT_TRUE(iter.valid());
        ASSERT_STREQ("8700", (*iter).to_string().c_str());
    }

    // find prefix "9999"
    {
        auto iter = decoder.upper_bound("9999");
        ASSERT_FALSE(iter.valid());
    }
}

} // namespace starrocks
