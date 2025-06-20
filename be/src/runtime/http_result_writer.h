#pragma once
#include "runtime/buffer_control_result_writer.h"
#include "runtime/result_writer.h"
#include "util/raw_container.h"

namespace starrocks {

class ExprContext;
class BufferControlBlock;
class RuntimeProfile;
using TFetchDataResultPtr = std::unique_ptr<TFetchDataResult>;
using TFetchDataResultPtrs = std::vector<TFetchDataResultPtr>;
// convert the row batch to mysql protocol row
class HttpResultWriter final : public BufferControlResultWriter {
public:
    HttpResultWriter(BufferControlBlock* sinker, const std::vector<ExprContext*>& output_expr_ctxs,
                     RuntimeProfile* parent_profile, TResultSinkFormatType::type format_type);

    Status init(RuntimeState* state) override;

    Status append_chunk(Chunk* chunk) override;

    StatusOr<TFetchDataResultPtrs> process_chunk(Chunk* chunk) override;

private:
    Status _transform_row_to_json(const Columns& column, int idx);

    const std::vector<ExprContext*>& _output_expr_ctxs;

    raw::RawString _row_str;

    const size_t _max_row_buffer_size = 1024 * 1024 * 1024;

    // result's format, right now just support json format
    TResultSinkFormatType::type _format_type;
};

} // namespace starrocks