#pragma once

#include <sys/un.h>

#include <array>
#include <cstdint>
#include <string>

namespace starrocks {

// Classless Inter-Domain Routing
class CIDR {
public:
    CIDR();
    bool reset(const std::string& cidr_str);
    bool contains(const CIDR& ip) const;
    static bool ip_to_int(const std::string& ip_str, uint32_t* value);

private:
    sa_family_t _family;
    std::array<std::uint8_t, 16> _address;
    std::uint8_t _netmask_len;
};

} // end namespace starrocks
