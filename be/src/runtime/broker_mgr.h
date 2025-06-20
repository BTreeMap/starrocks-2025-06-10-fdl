#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

#include "gen_cpp/Types_types.h"
#include "util/hash_util.hpp"

namespace starrocks {

class ExecEnv;

class BrokerMgr {
public:
    BrokerMgr(ExecEnv* exec_env);
    ~BrokerMgr();
    void init();
    const std::string& get_client_id(const TNetworkAddress& address);

private:
    void ping(const TNetworkAddress& addr);
    void ping_worker();

    ExecEnv* _exec_env;
    std::string _client_id;
    std::mutex _mutex;
    std::unordered_set<TNetworkAddress> _broker_set;
    bool _thread_stop;
    std::thread _ping_thread;
};

} // namespace starrocks
