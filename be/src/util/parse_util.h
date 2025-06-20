#pragma once

#include <boost/cstdint.hpp>
#include <string>

#include "common/statusor.h"

namespace starrocks {

// Utility class for parsing information from strings.
class ParseUtil {
public:
    // Parses mem_spec_str and returns the memory size in bytes.
    // Accepted formats:
    // '<int>[bB]?'  -> bytes (default if no unit given)
    // '<float>[mM]' -> megabytes
    // '<float>[gG]' -> in gigabytes
    // '<int>%'      -> in percent of memory_limit
    // Return 0 if mem_spec_str is empty.
    // Return -1, means no limit and will automatically adjust
    // The caller needs to handle other legitimate negative values.
    // If parsing mem_spec_str fails, it will return an error.
    static StatusOr<int64_t> parse_mem_spec(const std::string& mem_spec_str, const int64_t memory_limit);
};

} // namespace starrocks
