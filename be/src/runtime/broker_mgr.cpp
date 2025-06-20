#include "runtime/broker_mgr.h"

#include <sstream>

#include "common/config.h"
#include "gen_cpp/FileBrokerService_types.h"
#include "gen_cpp/TFileBrokerService.h"
#include "runtime/client_cache.h"
#include "runtime/exec_env.h"
#include "service/backend_options.h"
#include "util/misc.h"
#include "util/starrocks_metrics.h"
#include "util/thread.h"
#include "util/thrift_rpc_helper.h"

namespace starrocks {

BrokerMgr::BrokerMgr(ExecEnv* exec_env)
        : _exec_env(exec_env), _thread_stop(false), _ping_thread(&BrokerMgr::ping_worker, this) {
    Thread::set_thread_name(_ping_thread, "broker_hrtbeat"); // broker heart beat
    REGISTER_GAUGE_STARROCKS_METRIC(broker_count, [this]() {
        std::lock_guard<std::mutex> l(_mutex);
        return _broker_set.size();
    });
}

BrokerMgr::~BrokerMgr() {
    _thread_stop = true;
    _ping_thread.join();
}

void BrokerMgr::init() {
    std::stringstream ss;
    ss << BackendOptions::get_localhost() << ":" << config::be_port;
    _client_id = ss.str();
}

const std::string& BrokerMgr::get_client_id(const TNetworkAddress& address) {
    std::lock_guard<std::mutex> l(_mutex);
    _broker_set.insert(address);
    return _client_id;
}

void BrokerMgr::ping(const TNetworkAddress& addr) {
    TBrokerPingBrokerRequest request;

    request.__set_version(TBrokerVersion::VERSION_ONE);
    request.__set_clientId(_client_id);

    TBrokerOperationStatus response;
    Status rpc_status;

    // 500ms is enough
    rpc_status = ThriftRpcHelper::rpc<TFileBrokerServiceClient>(
            addr, [&response, &request](BrokerServiceConnection& client) { client->ping(response, request); }, 500);
    if (!rpc_status.ok()) {
        LOG(WARNING) << "Broker ping failed, broker:" << addr << " failed:" << rpc_status;
    }
}

void BrokerMgr::ping_worker() {
    while (!_thread_stop) {
        std::vector<TNetworkAddress> addresses;
        {
            std::lock_guard<std::mutex> l(_mutex);
            for (auto& addr : _broker_set) {
                addresses.emplace_back(addr);
            }
        }
        for (auto& addr : addresses) {
            ping(addr);
        }
        nap_sleep(5, [this] { return _thread_stop; });
    }
}

} // namespace starrocks
