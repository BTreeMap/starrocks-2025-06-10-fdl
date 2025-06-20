#pragma once

#include <pthread.h>

#include <future>
#include <mutex>
#include <string>
#include <vector>

#include "common/status.h"
#include "runtime/base_load_path_mgr.h"

namespace starrocks {

class TUniqueId;
class ExecEnv;

// In every directory, '.trash' directory is used to save data need to delete
// daemon thread is check no used directory to delete
class LoadPathMgr final : public BaseLoadPathMgr {
public:
    LoadPathMgr(ExecEnv* env);

    ~LoadPathMgr() override;

    Status init() override;

    Status allocate_dir(const std::string& db, const std::string& label, std::string* prefix) override;

    void get_load_data_path(std::vector<std::string>* data_paths) override;

    Status get_load_error_file_name(const TUniqueId& fragment_instance_id, std::string* error_path) override;
    std::string get_load_error_absolute_path(const std::string& file_path) override;

    std::string get_load_rejected_record_absolute_path(const std::string& rejected_record_dir, const std::string& db,
                                                       const std::string& label, const int64_t id,
                                                       const TUniqueId& fragment_instance_id) override;

private:
    bool is_too_old(time_t cur_time, const std::string& label_dir, int64_t reserve_hours);
    void clean_one_path(const std::string& path);
    void clean_error_log();
    void clean();
    void process_path(time_t now, const std::string& path, int64_t reserve_hours);
    std::future<bool>& stop_future() { return _stop_future; }
    static void* cleaner(void* param);

    ExecEnv* _exec_env;
    std::mutex _lock;
    std::vector<std::string> _path_vec;
    std::promise<bool> _stop;
    std::future<bool> _stop_future;
    int _idx;
    pthread_t _cleaner_id = 0;
    uint32_t _next_shard;
};

} // namespace starrocks
