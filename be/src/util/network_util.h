#pragma once

#include <sys/un.h>

#include <string>
#include <vector>

#include "common/status.h"
#include "gen_cpp/Types_types.h"

namespace starrocks {

class InetAddress {
public:
    InetAddress(std::string ip, sa_family_t family, bool is_loopback);
    bool is_loopback() const;
    std::string get_host_address() const;
    bool is_ipv6() const;

private:
    std::string _ip_addr;
    sa_family_t _family;
    bool _is_loopback;
};

// Looks up all IP addresses associated with a given hostname. Returns
// an error status if any system call failed, otherwise OK. Even if OK
// is returned, addresses may still be of zero length.
Status hostname_to_ip(const std::string& host, std::string& ip);
Status hostname_to_ip(const std::string& host, std::string& ip, bool ipv6);
Status hostname_to_ipv4(const std::string& host, std::string& ip);
Status hostname_to_ipv6(const std::string& host, std::string& ip);

bool is_valid_ip(const std::string& ip);

// Sets the output argument to the system defined hostname.
// Returns OK if a hostname can be found, false otherwise.
Status get_hostname(std::string* hostname);

Status get_hosts(std::vector<InetAddress>* hosts);

// Utility method because Thrift does not supply useful constructors
TNetworkAddress make_network_address(const std::string& hostname, int port);

Status get_inet_interfaces(std::vector<std::string>* interfaces, bool include_ipv6 = false);

std::string get_host_port(const std::string& host, int port);

} // namespace starrocks
