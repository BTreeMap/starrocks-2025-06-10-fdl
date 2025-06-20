#pragma once

#include "service/backend_base.h"

namespace starrocks {

class AgentServer;

// This class just forward rpc requests to actual handlers, used
// to bind multiple services on single port.
class BackendService : public BackendServiceBase {
public:
    explicit BackendService(ExecEnv* exec_env);

    ~BackendService() override;

    void submit_tasks(TAgentResult& return_value, const std::vector<TAgentTaskRequest>& tasks) override;

    void make_snapshot(TAgentResult& return_value, const TSnapshotRequest& snapshot_request) override;

    void release_snapshot(TAgentResult& return_value, const std::string& snapshot_path) override;

    void publish_cluster_state(TAgentResult& result, const TAgentPublishRequest& request) override;

    void get_tablet_stat(TTabletStatResult& result) override;

    void get_tablets_info(TGetTabletsInfoResult& result_, const TGetTabletsInfoRequest& request) override;

private:
    AgentServer* _agent_server;
};

} // namespace starrocks
