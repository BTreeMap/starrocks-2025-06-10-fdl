#pragma once

#include <cstdint>

#include "http/http_handler.h"

namespace starrocks {

class ChecksumAction : public HttpHandler {
public:
    ChecksumAction() = default;
    ~ChecksumAction() override = default;

    void handle(HttpRequest* req) override;

private:
    int64_t _do_checksum(int64_t tablet_id, int64_t version);
};

} // end namespace starrocks
