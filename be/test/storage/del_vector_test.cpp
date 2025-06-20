#include "storage/del_vector.h"

#include <gtest/gtest.h>

namespace starrocks {

// NOLINTNEXTLINE
TEST(DelVector, testLoadSave) {
    DelVector dv;
    dv.set_empty();
    ASSERT_TRUE(dv.empty());
    std::shared_ptr<DelVector> ndv;
    std::vector<uint32_t> dels = {1, 3, 5, 7, 90000};
    dv.add_dels_as_new_version(dels, 1, &ndv);
    ASSERT_FALSE(ndv->empty());
    std::string raw = ndv->save();
    DelVector dv2;
    ASSERT_TRUE(dv2.load(1, raw.data(), raw.size()).ok());
    ASSERT_GT(dv2.memory_usage(), 0);
    ASSERT_EQ(dv2.cardinality(), dels.size());
};

} // namespace starrocks
