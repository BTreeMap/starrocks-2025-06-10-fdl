#include "http/action/health_action.h"

#include <sstream>
#include <string>

#include "common/tracer.h"
#include "http/http_channel.h"
#include "http/http_headers.h"
#include "http/http_request.h"
#include "http/http_status.h"

namespace starrocks {

const static std::string HEADER_JSON = "application/json";

HealthAction::HealthAction(ExecEnv* exec_env) : _exec_env(exec_env) {}

void HealthAction::handle(HttpRequest* req) {
    auto scoped_span = trace::Scope(Tracer::Instance().start_trace("http_handle_health"));
    std::stringstream ss;
    ss << "{";
    ss << R"("status": "OK",)";
    ss << R"("msg": "To Be Added")";
    ss << "}";
    std::string result = ss.str();

    req->add_output_header(HttpHeaders::CONTENT_TYPE, HEADER_JSON.c_str());
    HttpChannel::send_reply(req, HttpStatus::OK, result);
#if 0
    HttpResponse response(HttpStatus::OK, HEADER_JSON, &result);
    channel->send_response(response);
#endif
}

} // end namespace starrocks
