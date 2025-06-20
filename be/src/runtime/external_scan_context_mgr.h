#pragma once

#include <condition_variable>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "common/status.h"
#include "gen_cpp/Types_types.h"
#include "runtime/exec_env.h"

namespace starrocks {

struct ScanContext {
public:
    TUniqueId query_id;
    TUniqueId fragment_instance_id;
    int64_t offset = 0;
    // use this access_time to clean zombie context
    time_t last_access_time = 0;
    std::string context_id;
    short keep_alive_min = 0;
    ScanContext(std::string id) : context_id(std::move(id)) {}
    ScanContext(const TUniqueId& fragment_id, int64_t offset) : fragment_instance_id(fragment_id), offset(offset) {}
};

class ExternalScanContextMgr {
public:
    ExternalScanContextMgr(ExecEnv* exec_env);

    ~ExternalScanContextMgr();

    Status create_scan_context(std::shared_ptr<ScanContext>* p_context);

    Status get_scan_context(const std::string& context_id, std::shared_ptr<ScanContext>* p_context);

    Status clear_scan_context(const std::string& context_id);

private:
    ExecEnv* _exec_env;
    std::map<std::string, std::shared_ptr<ScanContext>> _active_contexts;
    void gc_expired_context();
    std::unique_ptr<std::thread> _keep_alive_reaper;

    std::mutex _lock;
    std::condition_variable _cv;
    bool _closing = false;
};

} // namespace starrocks
