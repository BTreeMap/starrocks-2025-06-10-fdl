#pragma once

#include <boost/cstdint.hpp>
#include <string>

#include "common/logging.h"

namespace starrocks {

// Provides the amount of physical memory available.
// Populated from /proc/meminfo.
// TODO: Combine mem-info, cpu-info and disk-info into hardware-info?
class MemInfo {
public:
    // Initialize MemInfo.
    static void init();

    // Get total physical memory in bytes
    static int64_t physical_mem() {
        DCHECK(_s_initialized);
        return _s_physical_mem;
    }

    static std::string debug_string();

private:
    static void set_memlimit_if_container();

    static bool _s_initialized;
    static int64_t _s_physical_mem;
};

} // namespace starrocks
