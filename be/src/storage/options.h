#pragma once

#include <string>
#include <utility>
#include <vector>

#include "fs/fs.h"
#include "storage/lake/location_provider.h"
#include "storage/olap_define.h"
#include "util/uid_util.h"

namespace starrocks {

class MemTracker;

struct StorePath {
    StorePath() = default;
    explicit StorePath(std::string path_) : path(std::move(path_)) {}
    std::string path;
    TStorageMedium::type storage_medium{TStorageMedium::HDD};
};

// parse a single root path of storage_root_path
Status parse_root_path(const std::string& root_path, StorePath* path);

Status parse_conf_store_paths(const std::string& config_path, std::vector<StorePath>* path);

Status parse_conf_datacache_paths(const std::string& config_path, std::vector<std::string>* paths);

struct EngineOptions {
    // list paths that tablet will be put into.
    std::vector<StorePath> store_paths;
    // BE's UUID. It will be reset every time BE restarts.
    UniqueId backend_uid{0, 0};
    MemTracker* compaction_mem_tracker = nullptr;
    MemTracker* update_mem_tracker = nullptr;
    // if start as cn, no need to write cluster id
    bool need_write_cluster_id = true;
};

// Options only applies to cloud-native table r/w IO
struct LakeIOOptions {
    // Cache remote file locally on read requests.
    // This options can be ignored if the underlying filesystem does not support local cache.
    bool fill_data_cache = false;

    bool skip_disk_cache = false;
    // Specify different buffer size for different read scenarios
    int64_t buffer_size = -1;
    bool fill_metadata_cache = false;
    bool use_page_cache = false;
    bool cache_file_only = false; // only used for CACHE SELECT
    std::shared_ptr<FileSystem> fs;
    std::shared_ptr<starrocks::lake::LocationProvider> location_provider;
};

} // namespace starrocks
