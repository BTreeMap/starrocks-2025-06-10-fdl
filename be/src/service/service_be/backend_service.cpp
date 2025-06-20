#include "backend_service.h"

#include "agent/agent_server.h"
#include "agent/task_worker_pool.h"
#include "storage/storage_engine.h"
#include "storage/tablet_manager.h"

namespace starrocks {

BackendService::BackendService(ExecEnv* exec_env)
        : BackendServiceBase(exec_env), _agent_server(exec_env->agent_server()) {}

BackendService::~BackendService() = default;

void BackendService::get_tablet_stat(TTabletStatResult& result) {
    StorageEngine::instance()->tablet_manager()->get_tablet_stat(&result);
}

void BackendService::submit_tasks(TAgentResult& return_value, const std::vector<TAgentTaskRequest>& tasks) {
    _agent_server->submit_tasks(return_value, tasks);
}

void BackendService::make_snapshot(TAgentResult& return_value, const TSnapshotRequest& snapshot_request) {
    _agent_server->make_snapshot(return_value, snapshot_request);
}

void BackendService::release_snapshot(TAgentResult& return_value, const std::string& snapshot_path) {
    _agent_server->release_snapshot(return_value, snapshot_path);
}

void BackendService::publish_cluster_state(TAgentResult& result, const TAgentPublishRequest& request) {
    _agent_server->publish_cluster_state(result, request);
}

void BackendService::get_tablets_info(TGetTabletsInfoResult& result_, const TGetTabletsInfoRequest& request) {
    result_.__set_report_version(curr_report_version());
    result_.__isset.tablets = true;
    TStatus t_status;
    Status st_report = StorageEngine::instance()->tablet_manager()->report_all_tablets_info(&result_.tablets);
    if (!st_report.ok()) {
        LOG(WARNING) << "Fail to get all tablets info, err=" << st_report.to_string();
    }
    st_report.to_thrift(&t_status);
    result_.status = t_status;
}

} // namespace starrocks
