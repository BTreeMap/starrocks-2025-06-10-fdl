#pragma once

#include "storage/olap_common.h"
#include "storage/olap_define.h"
#include "storage/storage_engine.h"
#include "storage/tablet_manager.h"
#include "storage/txn_manager.h"
#include "util/starrocks_metrics.h"

namespace starrocks {

// base class for storage engine
// add "Engine" as task prefix to prevent duplicate name with agent task
class EngineTask {
public:
    virtual Status execute() { return Status::OK(); }
    virtual ~EngineTask() = default;
};

} // end namespace starrocks
