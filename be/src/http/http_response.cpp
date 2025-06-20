#include "http/http_response.h"

namespace starrocks {

static std::string s_text_content_type = "text/plain; charset=UTF-8";

HttpResponse::HttpResponse(const HttpStatus& status)
        : _status(status), _content_type(s_text_content_type), _content(nullptr) {}

HttpResponse::HttpResponse(const HttpStatus& status, const std::string* content)
        : _status(status), _content_type(s_text_content_type), _content(content) {}

HttpResponse::HttpResponse(const HttpStatus& status, const std::string& type, const std::string* content)
        : _status(status), _content_type(type), _content(content) {}

void HttpResponse::add_header(const std::string& key, const std::string& value) {
    _custom_headers[key].push_back(value);
}

} // namespace starrocks
