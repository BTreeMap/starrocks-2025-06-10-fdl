#pragma once

#include <utility>

#include "common/status.h"
#include "util/byte_buffer.h"

namespace starrocks {

class MessageBodySink {
public:
    virtual ~MessageBodySink() = default;
    virtual Status append(const char* data, size_t size) = 0;
    virtual Status append(ByteBufferPtr&& buf) = 0;
    // called when all data has been appended
    virtual Status finish() { return Status::OK(); }
    // called when read HTTP failed
    virtual void cancel(const Status& status) {}
    // check if all data has consume
    virtual bool exhausted() { return false; }
};

// write message to a local file
class MessageBodyFileSink : public MessageBodySink {
public:
    MessageBodyFileSink(std::string path) : _path(std::move(path)) {}
    ~MessageBodyFileSink() override;

    Status open();

    Status append(const char* data, size_t size) override;
    Status append(ByteBufferPtr&& buf) override;
    Status finish() override;
    void cancel(const Status& status) override;

private:
    std::string _path;
    int _fd = -1;
};

} // namespace starrocks
