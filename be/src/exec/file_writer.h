#pragma once

#include <cstdint>

#include "common/status.h"

namespace starrocks {

// NOTE: Deprecated, use `FileSystem::WritableFile` instead.
class FileWriter {
public:
    virtual ~FileWriter() = default;

    virtual Status open() = 0;

    // Writes up to count bytes from the buffer pointed buf to the file.
    // NOTE: the number of bytes written may be less than count if.
    virtual Status write(const uint8_t* buf, size_t buf_len, size_t* written_len) = 0;

    virtual Status close() = 0;
};

} // end namespace starrocks
