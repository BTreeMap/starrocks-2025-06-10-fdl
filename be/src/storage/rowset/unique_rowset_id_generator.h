#pragma once

#include <atomic>

#include "storage/rowset/rowset_id_generator.h"
#include "util/spinlock.h"
#include "util/uid_util.h"

namespace starrocks {

class UniqueRowsetIdGenerator : public RowsetIdGenerator {
public:
    UniqueRowsetIdGenerator(const UniqueId& backend_uid);
    ~UniqueRowsetIdGenerator() override = default;

    RowsetId next_id() override;

    bool id_in_use(const RowsetId& rowset_id) const override;

    void release_id(const RowsetId& rowset_id) override;

private:
    mutable SpinLock _lock;
    const UniqueId _backend_uid;
    const int64_t _version = 2; // modify it when create new version id generator
    std::atomic<int64_t> _inc_id;
    std::unordered_set<int64_t> _valid_rowset_id_hi;

    UniqueRowsetIdGenerator(const UniqueRowsetIdGenerator&) = delete;
    const UniqueRowsetIdGenerator& operator=(const UniqueRowsetIdGenerator&) = delete;
}; // UniqueRowsetIdGenerator

} // namespace starrocks
