#pragma once
#include <atomic>
#include <thread>
#include <vector>

#include "gutil/macros.h"
#include "storage/options.h"

namespace starrocks {

class Daemon {
public:
    Daemon() = default;
    ~Daemon() = default;

    void init(bool as_cn, const std::vector<StorePath>& paths);
    void stop();
    bool stopped();

private:
    std::atomic<bool> _stopped{false};

    std::vector<std::thread> _daemon_threads;
    DISALLOW_COPY(Daemon);
};

} // namespace starrocks
