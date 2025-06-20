#pragma once

#include "gen_cpp/AgentService_types.h"
#include "storage/olap_define.h"
#include "storage/task/engine_task.h"

namespace starrocks {

// base class for storage engine
// add "Engine" as task prefix to prevent duplicate name with agent task
class EngineAlterTabletTask : public EngineTask {
public:
    Status execute() override;

public:
    EngineAlterTabletTask(MemTracker* mem_tracker, const TAlterTabletReqV2& alter_tablet_request);
    ~EngineAlterTabletTask() override = default;

private:
    std::unique_ptr<MemTracker> _mem_tracker;

    const TAlterTabletReqV2& _alter_tablet_req;

}; // EngineTask

} // namespace starrocks
