#include "storage/rowset/bitshuffle_page.h"

#include "gutil/strings/substitute.h"
#include "storage/rowset/common.h"

namespace starrocks {

std::string bitshuffle_error_msg(int64_t err) {
    switch (err) {
    case -1:
        return "Failed to allocate memory";
    case -11:
        return "Missing SSE";
    case -12:
        return "Missing AVX";
    case -80:
        return "Input size not a multiple of 8";
    case -81:
        return "block_size not multiple of 8";
    case -91:
        return "Decompression error, wrong number of bytes processed";
    default:
        return strings::Substitute("Error internal to compression routine with error code $0", err);
    }
}

} // namespace starrocks
