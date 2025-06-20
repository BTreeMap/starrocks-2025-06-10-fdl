#pragma once

#include <bvar/variable.h>

#include <string>

#include "http/http_handler.h"

namespace starrocks {

class ExecEnv;
class HttpRequest;
class MetricRegistry;

typedef void (*MockFunc)(const std::string&);

class MetricsAction : public HttpHandler {
public:
    explicit MetricsAction(MetricRegistry* metrics) : _metrics(metrics), _mock_func(nullptr) {}
    // for tests
    explicit MetricsAction(MetricRegistry* metrics, MockFunc func) : _metrics(metrics), _mock_func(func) {}
    ~MetricsAction() override = default;

    void handle(HttpRequest* req) override;

private:
    MetricRegistry* _metrics;
    MockFunc _mock_func;
    bvar::DumpOptions _options;
};

} // namespace starrocks
