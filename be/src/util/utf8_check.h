#pragma once

#include <cstddef>

#include "simdutf.h"

namespace starrocks {
// check utf8 code using simd instructions
// Return true - success,  false fail
inline bool validate_utf8(const char* src, size_t len) {
    return simdutf::validate_utf8(src, len);
}
// chech utf8 use naive c++
bool validate_utf8_naive(const char* data, size_t len);
} // namespace starrocks
