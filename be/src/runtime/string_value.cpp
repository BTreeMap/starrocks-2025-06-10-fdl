#include "runtime/string_value.h"

#include <cstring>

namespace starrocks {

std::string StringValue::debug_string() const {
    return {ptr, len};
}

std::string StringValue::to_string() const {
    return {ptr, len};
}

std::ostream& operator<<(std::ostream& os, const StringValue& string_value) {
    return os << string_value.debug_string();
}

std::size_t operator-(const StringValue& v1, const StringValue& v2) {
    return 0;
}

} // namespace starrocks
