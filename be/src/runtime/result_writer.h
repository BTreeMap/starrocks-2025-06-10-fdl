#pragma once

#include <stdexcept>

#include "column/column.h"
#include "column/vectorized_fwd.h"
#include "common/status.h"
#include "common/statusor.h"
#include "gen_cpp/InternalService_types.h"
#include "gen_cpp/PlanNodes_types.h"

namespace starrocks {

class Status;
class RuntimeState;
using TFetchDataResultPtr = std::unique_ptr<TFetchDataResult>;
using TFetchDataResultPtrs = std::vector<TFetchDataResultPtr>;

// abstract class of the result writer
class ResultWriter {
public:
    ResultWriter() = default;
    virtual ~ResultWriter() = default;

    virtual Status init(RuntimeState* state) = 0;

    virtual Status open(RuntimeState* state) { return Status::OK(); };

    // convert one chunk to mysql result and
    // append this chunk to the result sink
    // used in non-pipeline engine
    virtual Status append_chunk(Chunk* chunk) = 0;

    // used in pipeline-engine no io block
    virtual Status add_to_write_buffer(Chunk* chunk) { return Status::NotSupported("Not Implemented"); }
    virtual bool is_full() const { throw std::runtime_error("not implements is full in ResultWriter"); }
    virtual void cancel() { throw std::runtime_error("not implements is full in ResultWriter"); }

    virtual Status close() = 0;

    int64_t get_written_rows() const { return _written_rows; }

protected:
    // used in pipeline engine,
    // the former transform input chunk into multiple TFetchDataResult, the latter add TFetchDataResult
    // to queue whose consumers are rpc threads that invoke fetch_data rpc.
    // TODO: Avoid serialization overhead.
    virtual StatusOr<TFetchDataResultPtrs> process_chunk(Chunk* chunk) {
        return Status::NotSupported("Not Implemented");
    }

protected:
    int64_t _written_rows = 0; // number of rows written
};

} // namespace starrocks
