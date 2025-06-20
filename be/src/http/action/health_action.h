#pragma once

#include "http/http_handler.h"

namespace starrocks {

class ExecEnv;

// Get BE health state from http API.
class HealthAction : public HttpHandler {
public:
    explicit HealthAction(ExecEnv* exec_env);

    ~HealthAction() override = default;

    void handle(HttpRequest* req) override;

private:
    [[maybe_unused]] ExecEnv* _exec_env;
};

} // end namespace starrocks
