#pragma once

#include <cstdint>
#include <limits>

#include "util/alignment.h"

enum { BINARY_DICT_PAGE_HEADER_SIZE = 4 };
enum { BITSHUFFLE_PAGE_HEADER_SIZE = 16 };

namespace starrocks {

// One segment file could store at most INT32_MAX rows,
// but due to array type, each column could store more than INT32_MAX values.
using rowid_t = uint32_t;
using ordinal_t = uint64_t;

} // namespace starrocks
