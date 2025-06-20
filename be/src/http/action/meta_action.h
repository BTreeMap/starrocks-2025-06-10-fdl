#pragma once

#include "common/status.h"
#include "http/http_handler.h"

namespace starrocks {

class ExecEnv;

enum META_TYPE {
    HEADER = 1,
};

// Get Meta Info
class MetaAction : public HttpHandler {
public:
    explicit MetaAction(META_TYPE meta_type) : _meta_type(meta_type) {}

    ~MetaAction() override = default;

    void handle(HttpRequest* req) override;

private:
    static Status _handle_header(HttpRequest* req, std::string* json_header);

    META_TYPE _meta_type;
};

} // end namespace starrocks
