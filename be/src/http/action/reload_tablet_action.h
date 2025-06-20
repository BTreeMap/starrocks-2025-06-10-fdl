#pragma once

#include "gen_cpp/AgentService_types.h"
#include "http/http_handler.h"

namespace starrocks {

class ExecEnv;

class ReloadTabletAction : public HttpHandler {
public:
    explicit ReloadTabletAction(ExecEnv* exec_env);

    ~ReloadTabletAction() override = default;

    void handle(HttpRequest* req) override;

private:
    void reload(const std::string& path, int64_t tablet_id, int32_t schema_hash, HttpRequest* req);

    [[maybe_unused]] ExecEnv* _exec_env;

}; // end class ReloadTabletAction

} // end namespace starrocks
