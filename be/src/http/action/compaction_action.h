#pragma once

#include <atomic>

#include "common/status.h"
#include "http/http_handler.h"

namespace starrocks {

enum CompactionActionType {
    SHOW_INFO = 1,
    RUN_COMPACTION = 2,
    SHOW_REPAIR = 3,
    SUBMIT_REPAIR = 4,
    SHOW_RUNNING_TASK = 5
};

// This action is used for viewing the compaction status.
// See compaction-action.md for details.
class CompactionAction : public HttpHandler {
public:
    explicit CompactionAction(CompactionActionType type) : _type(type) {}

    ~CompactionAction() override = default;

    void handle(HttpRequest* req) override;

    static Status do_compaction(uint64_t tablet_id, const std::string& compaction_type,
                                const std::string& rowset_ids_string);

private:
    Status _handle_show_compaction(HttpRequest* req, std::string* json_result);
    Status _handle_compaction(HttpRequest* req, std::string* json_result);
    Status _handle_show_repairs(HttpRequest* req, std::string* json_result);
    Status _handle_submit_repairs(HttpRequest* req, std::string* json_result);
    Status _handle_running_task(HttpRequest* req, std::string* json_result);

private:
    CompactionActionType _type;
    static std::atomic_bool _running;
};

} // end namespace starrocks
