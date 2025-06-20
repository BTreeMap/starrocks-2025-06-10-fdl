#include "util/cidr.h"

#include <arpa/inet.h>
#include <sys/socket.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <iterator>
#include <ostream>

#include "common/logging.h"
#include "gutil/strings/numbers.h"
#include "gutil/strings/split.h"

namespace starrocks {

CIDR::CIDR() = default;

constexpr std::uint8_t kIPv4Bits = 32;
constexpr std::uint8_t kIPv6Bits = 128;

bool CIDR::reset(const std::string& cidr_str) {
    auto slash = std::find(std::begin(cidr_str), std::end(cidr_str), '/');
    auto ip = (slash == std::end(cidr_str)) ? cidr_str : cidr_str.substr(0, slash - std::begin(cidr_str));

    if (inet_pton(AF_INET, ip.c_str(), _address.data())) {
        _family = AF_INET;
        _netmask_len = kIPv4Bits;
    } else if (inet_pton(AF_INET6, ip.c_str(), _address.data())) {
        _family = AF_INET6;
        _netmask_len = kIPv6Bits;
    } else {
        LOG(WARNING) << "Wrong CIDRIP format. network = " << cidr_str;
        return false;
    }

    if (slash == std::end(cidr_str)) {
        return true;
    }

    std::size_t pos;
    std::string suffix = std::string(slash + 1, std::end(cidr_str));
    int len;
    try {
        len = std::stoi(suffix, &pos);
    } catch (const std::exception& e) {
        LOG(WARNING) << "Wrong CIDR format. network = " << cidr_str << ", reason = " << e.what();
        return false;
    }

    if (pos != suffix.size()) {
        LOG(WARNING) << "Wrong CIDR format. network = " << cidr_str;
        return false;
    }

    if (len < 0 || len > _netmask_len) {
        LOG(WARNING) << "Wrong CIDR format. network = " << cidr_str;
        return false;
    }
    _netmask_len = len;
    return true;
}

bool CIDR::ip_to_int(const std::string& ip_str, uint32_t* value) {
    struct in_addr addr_v4;
    int flag = inet_aton(ip_str.c_str(), &addr_v4);
    if (flag == 1) {
        *value = ntohl(addr_v4.s_addr);
        return true;
    }
    struct in6_addr addr_v6;
    flag = inet_pton(AF_INET6, ip_str.c_str(), &addr_v6);
    if (flag == 1) {
        if (IN6_IS_ADDR_V4MAPPED(&addr_v6)) {
            *value = ntohl(*(reinterpret_cast<uint32_t*>(&addr_v6.s6_addr[12])));
            return true;
        }
        *value = static_cast<uint32_t>(addr_v6.s6_addr32[3]);
        return true;
    }
    return false;
}

bool CIDR::contains(const CIDR& ip) const {
    if ((_family != ip._family) || (_netmask_len > ip._netmask_len)) {
        return false;
    }
    auto bytes = _netmask_len / 8;
    auto cidr_begin = _address.cbegin();
    auto ip_begin = ip._address.cbegin();
    if (!std::equal(cidr_begin, cidr_begin + bytes, ip_begin, ip_begin + bytes)) {
        return false;
    }
    if ((_netmask_len % 8) == 0) {
        return true;
    }
    auto mask = (0xFF << (8 - (_netmask_len % 8))) & 0xFF;
    return (_address[bytes] & mask) == (ip._address[bytes] & mask);
}

} // end namespace starrocks
