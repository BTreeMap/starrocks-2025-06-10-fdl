#include "runtime/message_body_sink.h"

#include <fcntl.h>
#include <sys/stat.h>

namespace starrocks {

MessageBodyFileSink::~MessageBodyFileSink() {
    if (_fd >= 0) {
        close(_fd);
    }
}

Status MessageBodyFileSink::open() {
    _fd = ::open(_path.data(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (_fd < 0) {
        PLOG(WARNING) << "fail to open " << _path;
        return Status::InternalError("fail to open file");
    }
    return Status::OK();
}

Status MessageBodyFileSink::append(const char* data, size_t size) {
    auto written = ::write(_fd, data, size);
    if (written == size) {
        return Status::OK();
    }
    PLOG(WARNING) << "fail to write " << _path;
    return Status::InternalError("fail to write file");
}

Status MessageBodyFileSink::append(ByteBufferPtr&& buf) {
    return append(buf->ptr, buf->pos);
}

Status MessageBodyFileSink::finish() {
    if (::close(_fd) < 0) {
        PLOG(WARNING) << "fail to close " << _path;
        _fd = -1;
        return Status::InternalError("fail to close file");
    }
    _fd = -1;
    return Status::OK();
}

void MessageBodyFileSink::cancel(const Status& status) {
    unlink(_path.data());
}

} // namespace starrocks
