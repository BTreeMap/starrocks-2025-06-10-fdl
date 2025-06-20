#include "storage/olap_common.h"

#include "util/string_parser.hpp"

namespace starrocks {

void RowsetId::init(std::string_view rowset_id_str) {
    // for new rowsetid its a 48 hex string
    // if the len < 48, then it is an old format rowset id
    if (rowset_id_str.length() < 48) {
        StringParser::ParseResult result;
        auto high = StringParser::string_to_int<int64_t>(rowset_id_str.data(), rowset_id_str.size(), &result);
        DCHECK_EQ(StringParser::PARSE_SUCCESS, result);
        init(1, high, 0, 0);
    } else {
        int64_t high = 0;
        int64_t middle = 0;
        int64_t low = 0;
        from_hex(&high, rowset_id_str.substr(0, 16));
        from_hex(&middle, rowset_id_str.substr(16, 16));
        from_hex(&low, rowset_id_str.substr(32, 16));
        init(high >> 56, high & LOW_56_BITS, middle, low);
    }
}

} // namespace starrocks
