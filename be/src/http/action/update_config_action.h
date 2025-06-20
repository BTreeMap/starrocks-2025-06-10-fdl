#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>

#include "http/http_handler.h"
#include "runtime/exec_env.h"

namespace starrocks {

// Update BE config.
class UpdateConfigAction : public HttpHandler {
public:
    explicit UpdateConfigAction(ExecEnv* exec_env) : _exec_env(exec_env) { _instance.store(this); }
    ~UpdateConfigAction() override = default;

    void handle(HttpRequest* req) override;

    Status update_config(const std::string& name, const std::string& value);

    // hack to share update_config method with be_configs schema table sink
    static UpdateConfigAction* instance() { return _instance.load(); }

private:
    static std::atomic<UpdateConfigAction*> _instance;
    ExecEnv* _exec_env;
    std::once_flag _once_flag;
    std::unordered_map<std::string, std::function<Status()>> _config_callback;
};

} // namespace starrocks
