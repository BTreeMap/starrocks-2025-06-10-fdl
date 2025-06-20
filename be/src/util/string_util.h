#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp> // to_lower_copy
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace starrocks {

struct StringCaseHasher {
public:
    std::size_t operator()(const std::string& value) const {
        std::string lower_value = boost::algorithm::to_lower_copy(value);
        return std::hash<std::string>()(lower_value);
    }
};

struct StringCaseEqual {
public:
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        return strncasecmp(lhs.c_str(), rhs.c_str(), 0) == 0;
    }
};

struct StringCaseLess {
public:
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        size_t common_size = std::min(lhs.size(), rhs.size());
        auto cmp = strncasecmp(lhs.c_str(), rhs.c_str(), common_size);
        if (cmp == 0) {
            return lhs.size() < rhs.size();
        }
        return cmp < 0;
    }
};

size_t hash_of_path(const std::string& identifier, const std::string& path);

using StringCaseSet = std::set<std::string, StringCaseLess>;
using StringCaseUnorderedSet = std::unordered_set<std::string, StringCaseHasher, StringCaseEqual>;
template <class T>
using StringCaseMap = std::map<std::string, T, StringCaseLess>;
template <class T>
using StringCaseUnorderedMap = std::unordered_map<std::string, T, StringCaseHasher, StringCaseEqual>;

} // namespace starrocks
