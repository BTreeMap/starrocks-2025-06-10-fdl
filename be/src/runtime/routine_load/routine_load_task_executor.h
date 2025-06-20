#pragma once

#include <climits>
#include <functional>
#include <map>
#include <mutex>
#include <string_view>

#include "gen_cpp/internal_service.pb.h"
#include "runtime/routine_load/data_consumer_pool.h"
#include "util/starrocks_metrics.h"
#include "util/threadpool.h"
#include "util/uid_util.h"

namespace starrocks {

class ExecEnv;
class Status;
class StreamLoadContext;
class TRoutineLoadTask;

// A routine load task executor will receive routine load
// tasks from FE, put it to a fixed thread pool.
// The thread pool will process each task and report the result
// to FE finally.
class RoutineLoadTaskExecutor {
public:
    typedef std::function<void(StreamLoadContext*)> ExecFinishCallback;

    RoutineLoadTaskExecutor(ExecEnv* exec_env) : _exec_env(exec_env), _data_consumer_pool(10) {}

    ~RoutineLoadTaskExecutor() noexcept = default;

    Status init();
    void stop();

    // submit a routine load task
    Status submit_task(const TRoutineLoadTask& task);

    Status get_kafka_partition_meta(const PKafkaMetaProxyRequest& request, std::vector<int32_t>* partition_ids,
                                    int timeout, std::string* group_id);

    Status get_kafka_partition_offset(const PKafkaOffsetProxyRequest& request, std::vector<int64_t>* beginning_offsets,
                                      std::vector<int64_t>* latest_offsets, int timeout, std::string* group_id);

    Status get_pulsar_partition_meta(const PPulsarMetaProxyRequest& request, std::vector<std::string>* partitions);

    Status get_pulsar_partition_backlog(const PPulsarBacklogProxyRequest& request, std::vector<int64_t>* backlog_num);

private:
    // execute the task
    void exec_task(StreamLoadContext* ctx, DataConsumerPool* pool, const ExecFinishCallback& cb);

    void err_handler(StreamLoadContext* ctx, const Status& st, std::string_view err_msg);

    ExecEnv* _exec_env;
    std::unique_ptr<ThreadPool> _thread_pool;
    DataConsumerPool _data_consumer_pool;

    std::mutex _lock;
    // task id -> load context
    std::unordered_map<UniqueId, StreamLoadContext*> _task_map;
};

} // namespace starrocks
