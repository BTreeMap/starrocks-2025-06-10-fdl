#pragma once

#include <memory>
#include <mutex>

#include "agent/status.h"
#include "common/statusor.h"
#include "gen_cpp/HeartbeatService.h"
#include "gen_cpp/Status_types.h"
#include "gutil/macros.h"
#include "runtime/exec_env.h"
#include "storage/olap_define.h"
#include "thrift/transport/TTransportUtils.h"

namespace starrocks {

class StorageEngine;
class Status;
class ThriftServer;

class HeartbeatServer : public HeartbeatServiceIf {
public:
    HeartbeatServer();
    ~HeartbeatServer() override = default;

    virtual void init_cluster_id_or_die();

    // Master send heartbeat to this server
    //
    // Input parameters:
    // * master_info: The struct of master info, contains host ip and port
    //
    // Output parameters:
    // * heartbeat_result: The result of heartbeat set
    void heartbeat(THeartbeatResult& heartbeat_result, const TMasterInfo& master_info) override;

    DISALLOW_COPY_AND_MOVE(HeartbeatServer);

private:
    enum CmpResult {
        kUnchanged,
        kNeedUpdate,
        kNeedUpdateAndReport,
    };

    StatusOr<CmpResult> compare_master_info(const TMasterInfo& master_info);

    // New function to print TMasterInfo object as a string
    std::string print_master_info(const TMasterInfo& master_info) const;

    StorageEngine* _olap_engine;
}; // class HeartBeatServer

StatusOr<std::unique_ptr<ThriftServer>> create_heartbeat_server(ExecEnv* exec_env, uint32_t heartbeat_server_port,
                                                                uint32_t worker_thread_num);
} // namespace starrocks
