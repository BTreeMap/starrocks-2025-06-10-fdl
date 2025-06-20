#pragma once

#include "gutil/strings/fastmem.h"
#include "util/bit_util.h"

namespace starrocks {

// Return the number of bytes necessary to store the given number of bits.
inline size_t BitmapSize(size_t num_bits) {
    return (num_bits + 7) / 8;
}
} // namespace starrocks
