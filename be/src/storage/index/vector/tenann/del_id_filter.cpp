#ifdef WITH_TENANN
#include "storage/index/vector/tenann/del_id_filter.h"

#include "storage/range.h"
#include "storage/roaring2range.h"
#include "tenann/common/type_traits.h"
#include "tenann/searcher/id_filter.h"

namespace starrocks {

DelIdFilter::DelIdFilter(const SparseRange<>& scan_range) : _row_bitmap(range2roaring(scan_range)) {}

bool DelIdFilter::IsMember(tenann::idx_t id) const {
    return _row_bitmap.contains(id);
}

} // namespace starrocks
#endif