#pragma once

#include <runtime/mem_tracker.h>

#include <string>

#include "http/http_handler.h"

namespace starrocks {

class ExecEnv;
class HttpRequest;

class MemoryMetricsAction : public HttpHandler {
public:
    explicit MemoryMetricsAction() = default;

    ~MemoryMetricsAction() override = default;

    void handle(HttpRequest* req) override;

private:
    void getMemoryMetricTree(MemTracker* memTracker, std::stringstream& result, int64_t total_size);
};

} // namespace starrocks
