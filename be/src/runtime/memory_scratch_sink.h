#pragma once

#include "common/global_types.h"
#include "common/status.h"
#include "exec/data_sink.h"
#include "gen_cpp/PlanNodes_types.h"
#include "gen_cpp/StarrocksExternalService_types.h"
#include "runtime/result_queue_mgr.h"
#include "util/blocking_queue.hpp"

namespace arrow {

class MemoryPool;
class RecordBatch;
class Schema;

} // namespace arrow

namespace starrocks {

class ObjectPool;
class ObjectPool;
class RuntimeState;
class RuntimeProfile;
class BufferControlBlock;
class ExprContext;
class ResultWriter;
class MemTracker;

// used to push data to blocking queue
class MemoryScratchSink : public DataSink {
public:
    // construct a buffer for the result need send to blocking queue.
    // row_desc used for convert RowBatch to TRowBatch
    // buffer_size is the buffer size allocated to each scan
    MemoryScratchSink(const RowDescriptor& row_desc, const std::vector<TExpr>& select_exprs,
                      const TMemoryScratchSink& sink);

    ~MemoryScratchSink() override;

    Status prepare(RuntimeState* state) override;

    Status open(RuntimeState* state) override;

    Status send_chunk(RuntimeState* state, Chunk* chunk) override;

    Status close(RuntimeState* state, Status exec_status) override;

    RuntimeProfile* profile() override { return _profile; }

    // only for ut
    std::shared_ptr<arrow::Schema> schema() { return _arrow_schema; }

    std::vector<TExpr> get_output_expr() { return _t_output_expr; }

    const RowDescriptor get_row_desc();

private:
    Status prepare_exprs(RuntimeState* state);

    void _prepare_id_to_col_name_map();

    // Owned by the RuntimeState.
    const RowDescriptor& _row_desc;
    std::shared_ptr<arrow::Schema> _arrow_schema;
    std::unordered_map<int64_t, std::string> _id_to_col_name;

    BlockQueueSharedPtr _queue;

    RuntimeProfile* _profile = nullptr; // Allocated from _pool

    // Owned by the RuntimeState.
    const std::vector<TExpr>& _t_output_expr;
    std::vector<ExprContext*> _output_expr_ctxs;
};
} // namespace starrocks
