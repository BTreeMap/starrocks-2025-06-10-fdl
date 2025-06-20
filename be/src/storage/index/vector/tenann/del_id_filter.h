#pragma once

#ifdef WITH_TENANN
#include "storage/del_vector.h"
#include "storage/range.h"
#include "tenann/common/type_traits.h"
#include "tenann/searcher/id_filter.h"

namespace starrocks {

class DelIdFilter final : public tenann::IdFilter {
public:
    explicit DelIdFilter(const SparseRange<>& scan_range);
    ~DelIdFilter() override = default;

    bool IsMember(tenann::idx_t id) const override;

private:
    Roaring _row_bitmap;
};

} // namespace starrocks
#endif
