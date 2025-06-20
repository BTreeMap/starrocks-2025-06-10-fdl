#include "util/blocking_queue.hpp"

#include <glog/logging.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <boost/thread.hpp>
#include <memory>
#include <thread>

namespace starrocks {

// NOLINTNEXTLINE
TEST(BlockingQueueTest, TestBasic) {
    int32_t i;
    BlockingQueue<int32_t> test_queue(5);
    ASSERT_TRUE(test_queue.blocking_put(1));
    ASSERT_TRUE(test_queue.blocking_put(2));
    ASSERT_TRUE(test_queue.blocking_put(3));
    ASSERT_TRUE(test_queue.blocking_get(&i));
    ASSERT_EQ(1, i);
    ASSERT_TRUE(test_queue.blocking_get(&i));
    ASSERT_EQ(2, i);
    ASSERT_TRUE(test_queue.blocking_get(&i));
    ASSERT_EQ(3, i);
}

// NOLINTNEXTLINE
TEST(BlockingQueueTest, TestGetFromShutdownQueue) {
    int64_t i;
    BlockingQueue<int64_t> test_queue(2);
    ASSERT_TRUE(test_queue.blocking_put(123));
    test_queue.shutdown();
    ASSERT_FALSE(test_queue.blocking_put(456));
    ASSERT_TRUE(test_queue.blocking_get(&i));
    ASSERT_EQ(123, i);
    ASSERT_FALSE(test_queue.blocking_get(&i));
}

class MultiThreadTest {
public:
    MultiThreadTest() : _queue(_iterations * _nthreads / 10), _num_inserters(_nthreads) {}

    void inserter_thread(int arg) {
        for (int i = 0; i < _iterations; ++i) {
            _queue.blocking_put(arg);
        }

        {
            std::lock_guard<std::mutex> guard(_lock);

            if (--_num_inserters == 0) {
                _queue.shutdown();
            }
        }
    }

    void RemoverThread() {
        for (int i = 0; i < _iterations; ++i) {
            int32_t arg;
            bool got = _queue.blocking_get(&arg);

            if (!got) {
                arg = -1;
            }

            {
                std::lock_guard<std::mutex> guard(_lock);
                _gotten[arg] = _gotten[arg] + 1;
            }
        }
    }

    void Run() {
        for (int i = 0; i < _nthreads; ++i) {
            _threads.push_back(std::make_shared<std::thread>([this, i] { inserter_thread(i); }));
            _threads.push_back(std::make_shared<std::thread>([this] { RemoverThread(); }));
        }

        // We add an extra thread to ensure that there aren't enough elements in
        // the queue to go around.  This way, we test removal after shutdown.
        _threads.push_back(std::make_shared<std::thread>([this] { RemoverThread(); }));

        for (auto& _thread : _threads) {
            _thread->join();
        }

        // Let's check to make sure we got what we should have.
        std::lock_guard<std::mutex> guard(_lock);

        for (int i = 0; i < _nthreads; ++i) {
            ASSERT_EQ(_iterations, _gotten[i]);
        }

        // And there were _nthreads * (_iterations + 1)  elements removed, but only
        // _nthreads * _iterations elements added.  So some removers hit the shutdown
        // case.
        ASSERT_EQ(_iterations, _gotten[-1]);
    }

private:
    typedef std::vector<std::shared_ptr<std::thread> > ThreadVector;

    int _iterations{10000};
    int _nthreads{5};
    BlockingQueue<int32_t> _queue;
    // Lock for _gotten and _num_inserters.
    std::mutex _lock;
    // Map from inserter thread id to number of consumed elements from that id.
    // Ultimately, this should map each thread id to _insertions elements.
    // Additionally, if the blocking_get returns false, this increments id=-1.
    std::map<int32_t, int> _gotten;
    // All inserter and remover threads.
    ThreadVector _threads;
    // Number of inserters which haven't yet finished inserting.
    int _num_inserters;
};

// NOLINTNEXTLINE
TEST(BlockingQueueTest, TestMultipleThreads) {
    MultiThreadTest test;
    test.Run();
}

} // namespace starrocks
