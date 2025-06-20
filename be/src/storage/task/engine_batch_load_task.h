#pragma once

#include <utility>
#include <vector>

#include "agent/status.h"
#include "gen_cpp/AgentService_types.h"
#include "gen_cpp/MasterService_types.h"
#include "storage/olap_common.h"
#include "storage/olap_define.h"
#include "storage/task/engine_task.h"

using namespace std;

namespace starrocks {

const uint32_t PUSH_MAX_RETRY = 1;
const uint32_t MAX_RETRY = 3;
const uint32_t DEFAULT_DOWNLOAD_TIMEOUT = 3600;
class StorageEngine;

class EngineBatchLoadTask : public EngineTask {
public:
    EngineBatchLoadTask(TPushReq& push_req, std::vector<TTabletInfo>* tablet_infos, int64_t signature,
                        AgentStatus* res_status, MemTracker* mem_tracker);
    ~EngineBatchLoadTask() override;

    Status execute() override;

private:
    // The initial function of pusher
    virtual AgentStatus _init();

    // The process of push data to olap engine
    //
    // Output parameters:
    // * tablet_infos: The info of pushed tablet after push data
    virtual AgentStatus _process();

    // Delete data of specified tablet according to delete conditions,
    // once delete_data command submit success, deleted data is not visible,
    // but not actually deleted util delay_delete_time run out.
    //
    // @param [in] request specify tablet and delete conditions
    // @param [out] tablet_info_vec return tablet lastest status, which
    //              include version info, row count, data size, etc
    virtual Status _delete_data(const TPushReq& request, vector<TTabletInfo>* tablet_info_vec);

    Status _push(const TPushReq& request, std::vector<TTabletInfo>* tablet_info_vec);

    std::unique_ptr<MemTracker> _mem_tracker;

    bool _is_init = false;
    TPushReq& _push_req;
    std::vector<TTabletInfo>* _tablet_infos;
    int64_t _signature;
    AgentStatus* _res_status;
}; // class Pusher
} // namespace starrocks
