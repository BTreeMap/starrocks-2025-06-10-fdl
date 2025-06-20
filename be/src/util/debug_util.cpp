#include "util/debug_util.h"

#include <iomanip>
#include <sstream>

#include "common/version.h"
#include "gen_cpp/types.pb.h"

#define PRECISION 2

#define SECOND (1000)

#define THOUSAND (1000)
#define MILLION (THOUSAND * 1000)

namespace starrocks {

std::string print_plan_node_type(const TPlanNodeType::type& type) {
    std::map<int, const char*>::const_iterator i;
    i = _TPlanNodeType_VALUES_TO_NAMES.find(type);

    if (i != _TPlanNodeType_VALUES_TO_NAMES.end()) {
        return i->second;
    }

    return "Invalid plan node type";
}

std::string get_build_version(bool compact) {
    std::stringstream ss;
    ss << STARROCKS_VERSION << "-" << STARROCKS_COMMIT_HASH << std::endl << "BuildType: " << STARROCKS_BUILD_TYPE;
    if (!compact) {
        ss << std::endl
           << "Build distributor id: " << STARROCKS_BUILD_DISTRO_ID << std::endl
           << "Build arch: " << STARROCKS_BUILD_ARCH << std::endl
           << "Built on " << STARROCKS_BUILD_TIME << " by " << STARROCKS_BUILD_USER << "@" << STARROCKS_BUILD_HOST;
    }

    return ss.str();
}

size_t get_build_version(char* buffer, size_t max_size) {
    size_t length = 0;
    length = snprintf(buffer + length, max_size - length, "%s ", STARROCKS_VERSION) + length;
    length = snprintf(buffer + length, max_size - length, "%s ", STARROCKS_BUILD_TYPE) + length;
    length = snprintf(buffer + length, max_size - length, "(build %s)\n", STARROCKS_COMMIT_HASH) + length;
    return length;
}

std::string get_short_version() {
    static std::string short_version(std::string(STARROCKS_VERSION) + "-" + STARROCKS_COMMIT_HASH);
    return short_version;
}

std::string get_version_string(bool compact) {
    std::stringstream ss;
    ss << " version " << get_build_version(compact);
    return ss.str();
}

std::string hexdump(const char* buf, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::uppercase;
    for (size_t i = 0; i < len; ++i) {
        ss << std::setfill('0') << std::setw(2) << ((uint16_t)buf[i] & 0xff);
    }
    return ss.str();
}

} // namespace starrocks
