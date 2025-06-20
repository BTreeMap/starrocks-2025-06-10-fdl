#pragma once

#include <memory>
#include <string>
#include <vector>

#include "agent/agent_common.h"
#include "gutil/macros.h"

namespace starrocks {

class ExecEnv;
class Status;
class TAgentTaskRequest;
class TAgentResult;
class TAgentPublishRequest;
class TSnapshotRequest;
class ThreadPool;

// Each method corresponds to one RPC from FE Master, see BackendService.
class AgentServer {
public:
    explicit AgentServer(ExecEnv* exec_env, bool is_compute_node);

    ~AgentServer();

    void init_or_die();

    void stop();

    void submit_tasks(TAgentResult& agent_result, const std::vector<TAgentTaskRequest>& tasks);

    void make_snapshot(TAgentResult& agent_result, const TSnapshotRequest& snapshot_request);

    void release_snapshot(TAgentResult& agent_result, const std::string& snapshot_path);

    void publish_cluster_state(TAgentResult& agent_result, const TAgentPublishRequest& request);

    void update_max_thread_by_type(int type, int new_val);

    // |type| should be one of `TTaskType::type`, didn't define type as  `TTaskType::type` because
    // I don't want to include the header file `gen_cpp/Types_types.h` here.
    //
    // Returns nullptr if `type` is not a valid value of `TTaskType::type`.
    ThreadPool* get_thread_pool(int type) const;

    void stop_task_worker_pool(TaskWorkerType type) const;

    DISALLOW_COPY_AND_MOVE(AgentServer);

private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

} // end namespace starrocks
