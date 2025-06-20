#include "util/parse_util.h"

#include <fmt/format.h>

#include "util/string_parser.hpp"

namespace starrocks {

StatusOr<int64_t> ParseUtil::parse_mem_spec(const std::string& mem_spec_str, const int64_t memory_limit) {
    bool is_percent = false;
    if (mem_spec_str.empty()) {
        return 0;
    }

    // Assume last character indicates unit or percent.
    int32_t number_str_len = mem_spec_str.size() - 1;
    int64_t multiplier = -1;

    // Look for accepted suffix character.
    switch (*mem_spec_str.rbegin()) {
    case 't':
    case 'T':
        // Terabytes.
        multiplier = 1024L * 1024L * 1024L * 1024L;
        break;
    case 'g':
    case 'G':
        // Gigabytes.
        multiplier = 1024L * 1024L * 1024L;
        break;
    case 'm':
    case 'M':
        // Megabytes.
        multiplier = 1024L * 1024L;
        break;
    case 'k':
    case 'K':
        // Kilobytes
        multiplier = 1024L;
        break;
    case 'b':
    case 'B':
        break;
    case '%':
        is_percent = true;
        break;
    default:
        // No unit was given. Default to bytes.
        number_str_len = mem_spec_str.size();
        break;
    }

    StringParser::ParseResult result;
    int64_t bytes;

    if (multiplier != -1) {
        // Parse float - MB or GB
        auto limit_val = StringParser::string_to_float<double>(mem_spec_str.data(), number_str_len, &result);
        if (result != StringParser::PARSE_SUCCESS) {
            return Status::InvalidArgument(fmt::format("Parse mem string: {}", mem_spec_str));
        }

        bytes = multiplier * limit_val;
    } else {
        // Parse int - bytes or percent
        auto limit_val = StringParser::string_to_int<int64_t>(mem_spec_str.data(), number_str_len, &result);
        if (result != StringParser::PARSE_SUCCESS) {
            return Status::InvalidArgument(fmt::format("Parse mem string: {}", mem_spec_str));
        }

        if (is_percent) {
            bytes = (static_cast<double>(limit_val) / 100.0) * memory_limit;
        } else {
            bytes = limit_val;
        }
    }

    return bytes;
}

} // namespace starrocks
