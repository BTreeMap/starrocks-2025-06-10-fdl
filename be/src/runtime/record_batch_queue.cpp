#include "runtime/record_batch_queue.h"

namespace starrocks {

void RecordBatchQueue::update_status(const Status& status) {
    if (status.ok()) {
        return;
    }
    {
        std::lock_guard<SpinLock> l(_status_lock);
        if (_status.ok()) {
            _status = status;
        }
    }
}

void RecordBatchQueue::shutdown() {
    _queue.shutdown();
}

} // namespace starrocks
