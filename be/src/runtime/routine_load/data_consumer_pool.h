#pragma once

#include <ctime>
#include <memory>
#include <mutex>
#include <thread>

#include "runtime/routine_load/data_consumer.h"

namespace starrocks {

class DataConsumer;
class DataConsumerGroup;
class Status;

// DataConsumerPool saves all available data consumer
// to be reused
class DataConsumerPool {
public:
    DataConsumerPool(int64_t max_pool_size)
            : _is_closed(std::make_shared<bool>(false)), _max_pool_size(max_pool_size) {}

    ~DataConsumerPool() {
        std::unique_lock<std::mutex> l(_lock);
        *_is_closed = true;
    }

    void stop();

    // get a already initialized consumer from cache,
    // if not found in cache, create a new one.
    Status get_consumer(StreamLoadContext* ctx, std::shared_ptr<DataConsumer>* ret);

    // get several consumers and put them into group
    Status get_consumer_grp(StreamLoadContext* ctx, std::shared_ptr<DataConsumerGroup>* ret);

    // return the consumer to the pool
    void return_consumer(const std::shared_ptr<DataConsumer>& consumer);
    // return the consumers in consumer group to the pool
    void return_consumers(DataConsumerGroup* grp);

    void start_bg_worker();

private:
    void _clean_idle_consumer_bg();

    std::mutex _lock;
    std::shared_ptr<bool> _is_closed;
    std::list<std::shared_ptr<DataConsumer>> _pool;
    int64_t _max_pool_size;

    std::thread _clean_idle_consumer_thread;
};

} // end namespace starrocks
