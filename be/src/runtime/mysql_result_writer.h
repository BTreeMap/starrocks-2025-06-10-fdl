#pragma once

#include "runtime/buffer_control_result_writer.h"

namespace starrocks {

class ExprContext;
class MysqlRowBuffer;
class BufferControlBlock;
class RuntimeProfile;
using TFetchDataResultPtr = std::unique_ptr<TFetchDataResult>;
using TFetchDataResultPtrs = std::vector<TFetchDataResultPtr>;
// convert the row batch to mysql protocol row
class MysqlResultWriter final : public BufferControlResultWriter {
public:
    MysqlResultWriter(BufferControlBlock* sinker, const std::vector<ExprContext*>& output_expr_ctxs,
                      bool is_binary_format, RuntimeProfile* parent_profile);

    ~MysqlResultWriter() override;

    Status init(RuntimeState* state) override;

    Status append_chunk(Chunk* chunk) override;

    StatusOr<TFetchDataResultPtrs> process_chunk(Chunk* chunk) override;

private:
    // this function is only used in non-pipeline engine
    StatusOr<TFetchDataResultPtr> _process_chunk(Chunk* chunk);

    const std::vector<ExprContext*>& _output_expr_ctxs;
    MysqlRowBuffer* _row_buffer;
    bool _is_binary_format;

    const size_t _max_row_buffer_size = 1024 * 1024 * 1024;
};

} // namespace starrocks
