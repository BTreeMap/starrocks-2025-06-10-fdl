#include "util/string_util.h"

#include "gutil/strings/split.h"
#include "util/hash_util.hpp"

namespace starrocks {

size_t hash_of_path(const std::string& identifier, const std::string& path) {
    size_t hash = std::hash<std::string>()(identifier);
    std::vector<std::string> path_parts = strings::Split(path, "/", strings::SkipWhitespace());
    for (auto& part : path_parts) {
        HashUtil::hash_combine<std::string>(hash, part);
    }
    return hash;
}

} // namespace starrocks
