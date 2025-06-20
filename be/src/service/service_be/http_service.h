#pragma once

#include <memory>

#include "common/status.h"
#include "util/concurrent_limiter.h"

namespace starrocks {

class ExecEnv;
class DataCache;
class EvHttpServer;
class HttpHandler;
class WebPageHandler;

// HTTP service for StarRocks BE
class HttpServiceBE {
public:
    HttpServiceBE(DataCache* cache_env, ExecEnv* env, int port, int num_threads);
    ~HttpServiceBE();

    Status start();
    void stop();
    void join();

private:
    DataCache* _cache_env;
    ExecEnv* _env;

    std::unique_ptr<EvHttpServer> _ev_http_server;
    std::unique_ptr<WebPageHandler> _web_page_handler;

    std::vector<HttpHandler*> _http_handlers;

    std::unique_ptr<ConcurrentLimiter> _http_concurrent_limiter;
};

} // namespace starrocks
