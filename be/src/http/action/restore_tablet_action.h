#pragma once

#include <map>
#include <mutex>

#include "common/status.h"
#include "gen_cpp/AgentService_types.h"
#include "http/http_handler.h"

namespace starrocks {

class ExecEnv;

class RestoreTabletAction : public HttpHandler {
public:
    explicit RestoreTabletAction(ExecEnv* exec_env);

    ~RestoreTabletAction() override = default;

    void handle(HttpRequest* req) override;

private:
    Status _handle(HttpRequest* req);

    Status _restore(const std::string& key, int64_t tablet_id, int32_t schema_hash);

    Status _reload_tablet(const std::string& key, const std::string& shard_path, int64_t tablet_id,
                          int32_t schema_hash);

    bool _get_latest_tablet_path_from_trash(int64_t tablet_id, int32_t schema_hash, std::string* path);

    bool _get_timestamp_and_count_from_schema_hash_path(const std::string& time_label, uint64_t* timestamp,
                                                        uint64_t* counter);

    void _clear_key(const std::string& key);

    Status _create_hard_link_recursive(const std::string& src, const std::string& dst);

    [[maybe_unused]] ExecEnv* _exec_env;
    std::mutex _tablet_restore_lock;
    // store all current restoring tablet_id + schema_hash
    // key: tablet_id + schema_hash
    // value: "" or tablet path in trash
    std::map<std::string, std::string> _tablet_path_map;
    bool _is_primary_key = false;
};

} // end namespace starrocks
