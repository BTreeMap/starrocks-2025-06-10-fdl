#pragma once

#include <functional>
#include <map>

#include "gen_cpp/PlanNodes_types.h"
#include "http/http_handler.h"
#include "runtime/client_cache.h"
#include "runtime/mem_tracker.h"
#include "runtime/message_body_sink.h"

namespace starrocks {

class ExecEnv;
class Status;
class StreamLoadContext;
class ConcurrentLimiter;

class StreamLoadAction : public HttpHandler {
public:
    explicit StreamLoadAction(ExecEnv* exec_env, ConcurrentLimiter* limiter);
    ~StreamLoadAction() override;

    void handle(HttpRequest* req) override;

    bool request_will_be_read_progressively() override { return true; }

    int on_header(HttpRequest* req) override;

    void on_chunk_data(HttpRequest* req) override;
    void free_handler_ctx(void* ctx) override;

private:
    Status _on_header(HttpRequest* http_req, StreamLoadContext* ctx);
    Status _handle(StreamLoadContext* ctx);
    Status _data_saved_path(HttpRequest* req, std::string* file_path);
    Status _execute_plan_fragment(StreamLoadContext* ctx);
    Status _process_put(HttpRequest* http_req, StreamLoadContext* ctx);

    Status _handle_batch_write(HttpRequest* http_req, StreamLoadContext* ctx);

private:
    ExecEnv* _exec_env;
    ConcurrentLimiter* _http_concurrent_limiter = nullptr;
};

} // namespace starrocks
