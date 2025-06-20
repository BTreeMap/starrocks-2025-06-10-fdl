#pragma once

#include "common/status.h"
#ifdef WITH_TENANN
#include "tenann/common/seq_view.h"
#include "tenann/searcher/id_filter.h"
#include "tenann/store/index_meta.h"
#endif

namespace starrocks {

class VectorIndexReader {
public:
    VectorIndexReader() = default;
    virtual ~VectorIndexReader() = default;

#ifdef WITH_TENANN
    virtual Status init_searcher(const tenann::IndexMeta& meta, const std::string& index_path) = 0;

    virtual Status search(tenann::PrimitiveSeqView query_vector, int k, int64_t* result_ids, uint8_t* result_distances,
                          tenann::IdFilter* id_filter = nullptr) = 0;
    virtual Status range_search(tenann::PrimitiveSeqView query_vector, int k, std::vector<int64_t>* result_ids,
                                std::vector<float>* result_distances, tenann::IdFilter* id_filter, float range,
                                int order) = 0;
#endif
};

} // namespace starrocks
