#pragma once

#include "gen_cpp/AgentService_types.h"
#include "storage/olap_define.h"
#include "storage/task/engine_task.h"

namespace starrocks {

// base class for storage engine
// add "Engine" as task prefix to prevent duplicate name with agent task
class EngineChecksumTask : public EngineTask {
public:
    Status execute() override;

    EngineChecksumTask(MemTracker* mem_tracker, TTabletId tablet_id, TVersion version, uint32_t* checksum);

    ~EngineChecksumTask() override = default;

private:
    Status _compute_checksum();

    std::unique_ptr<MemTracker> _mem_tracker;

    TTabletId _tablet_id;
    TVersion _version;
    uint32_t* _checksum;
}; // EngineTask

} // namespace starrocks
