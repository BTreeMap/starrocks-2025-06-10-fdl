#pragma once

#include <rocksdb/write_batch.h>

#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "common/status.h"
#include "storage/olap_common.h"

namespace rocksdb {
class DB;
class ColumnFamilyHandle;
} // namespace rocksdb

namespace starrocks {

using ColumnFamilyHandle = rocksdb::ColumnFamilyHandle;
using WriteBatch = rocksdb::WriteBatch;
class MemTracker;

class KVStore {
public:
    explicit KVStore(std::string root_path);

    virtual ~KVStore();

    Status init(bool read_only = false);

    Status get(ColumnFamilyIndex column_family_index, const std::string& key, std::string* value);

    Status put(ColumnFamilyIndex column_family_index, const std::string& key, const std::string& value);

    Status write_batch(WriteBatch* batch);

    Status remove(ColumnFamilyIndex column_family_index, const std::string& key);

    Status iterate(ColumnFamilyIndex column_family_index, const std::string& prefix,
                   std::function<StatusOr<bool>(std::string_view, std::string_view)> const& func,
                   int64_t timeout_sec = -1);

    Status iterate_range(ColumnFamilyIndex column_family_index, const std::string& lower_bound,
                         const std::string& upper_bound,
                         std::function<StatusOr<bool>(std::string_view, std::string_view)> const& func);

    const std::string& root_path() const { return _root_path; }

    Status compact();

    Status flushWAL();

    Status flushMemTable();

    std::string get_stats();

    bool get_live_sst_files_size(uint64_t* live_sst_files_size);

    std::string get_root_path();

    ColumnFamilyHandle* handle(ColumnFamilyIndex column_family_index) { return _handles[column_family_index]; }

    // Becayse `DeleteRange` provided by rocksdb will generate too many tomestones and it will slow down rocksdb.
    // So we provide an opt version `DeleteRange` named `OptDeleteRange` here, it will :
    // 1. scan and get keys to be deleted first.
    // 2. and then generate write batch with batch delete.
    Status OptDeleteRange(ColumnFamilyIndex column_family_index, const std::string& begin_key,
                          const std::string& end_key, WriteBatch* batch);

private:
    static int64_t calc_rocksdb_write_buffer_size(MemTracker* mem_tracker);

private:
    std::string _root_path;
    rocksdb::DB* _db;
    std::vector<rocksdb::ColumnFamilyHandle*> _handles;
};

} // namespace starrocks
