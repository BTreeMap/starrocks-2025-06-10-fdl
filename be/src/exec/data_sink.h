#pragma once

#include <utility>
#include <vector>

#include "column/vectorized_fwd.h"
#include "common/status.h"
#include "exec/exec_node.h"
#include "exec/pipeline/pipeline_builder.h"
#include "gen_cpp/DataSinks_types.h"
#include "gen_cpp/Exprs_types.h"
#include "runtime/query_statistics.h"

namespace starrocks {

class ObjectPool;
class RuntimeProfile;
class RuntimeState;
class TPlanFragmentExecParams;
class RowDescriptor;
class DataStreamSender;

namespace pipeline {
class UnifiedExecPlanFragmentParams;
}

// Superclass of all data sinks.
class DataSink {
public:
    DataSink() = default;
    virtual ~DataSink() = default;

    virtual Status init(const TDataSink& thrift_sink, RuntimeState* state);

    // Setup. Call before send(), Open(), or Close().
    // Subclasses must call DataSink::Prepare().
    virtual Status prepare(RuntimeState* state);

    // Setup. Call before send() or close().
    virtual Status open(RuntimeState* state) = 0;

    virtual Status send_chunk(RuntimeState* state, Chunk* chunk);

    // Releases all resources that were allocated in prepare()/send().
    // Further send() calls are illegal after calling close().
    // It must be okay to call this multiple times. Subsequent calls should
    // be ignored.
    virtual Status close(RuntimeState* state, Status exec_status) {
        _closed = true;
        return Status::OK();
    }

    // Creates a new data sink from thrift_sink. A pointer to the
    // new sink is written to *sink, and is owned by the caller.
    static Status create_data_sink(RuntimeState* state, const TDataSink& thrift_sink,
                                   const std::vector<TExpr>& output_exprs, const TPlanFragmentExecParams& params,
                                   int32_t sender_id, const RowDescriptor& row_desc, std::unique_ptr<DataSink>* sink);

    // Returns the runtime profile for the sink.
    virtual RuntimeProfile* profile() = 0;

    virtual void set_query_statistics(std::shared_ptr<QueryStatistics> statistics) {
        _query_statistics = std::move(statistics);
    }

    Status decompose_data_sink_to_pipeline(pipeline::PipelineBuilderContext* context, RuntimeState* state,
                                           pipeline::OpFactories prev_operators,
                                           const pipeline::UnifiedExecPlanFragmentParams& request,
                                           const TDataSink& thrift_sink, const std::vector<TExpr>& output_exprs);

private:
    OperatorFactoryPtr _create_exchange_sink_operator(pipeline::PipelineBuilderContext* context,
                                                      const TDataStreamSink& stream_sink,
                                                      const DataStreamSender* sender);

protected:
    RuntimeState* _runtime_state = nullptr;

    // Set to true after close() has been called. subclasses should check and set this in
    // close().
    bool _closed{false};

    // Maybe this will be transferred to BufferControlBlock.
    std::shared_ptr<QueryStatistics> _query_statistics;
};

} // namespace starrocks
