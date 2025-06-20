#pragma once

#include <fmt/compile.h>
#include <fmt/format.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "util/hash_util.hpp"
#include "util/integer_util.h"

namespace starrocks {

class LargeIntValue {
public:
    static std::string to_string(__int128 value) { return integer_to_string<__int128>(value); }
};

std::ostream& operator<<(std::ostream& os, __int128 const& value);

std::istream& operator>>(std::istream& is, __int128& value);

std::size_t hash_value(LargeIntValue const& value);

} // namespace starrocks
