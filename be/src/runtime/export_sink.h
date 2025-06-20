#pragma once

#include <vector>

#include "exec/data_sink.h"
#include "formats/csv/converter.h"
#include "formats/csv/output_stream_file.h"
#include "fs/fs.h"
#include "util/runtime_profile.h"

namespace starrocks {

class RowDescriptor;
class TExpr;
class RuntimeState;
class RuntimeProfile;
class ExprContext;
class MemTracker;
class FileWriter;
class Status;
class FileBuilder;

// This class is a sinker, which put export data to external storage by broker.
class ExportSink : public DataSink {
public:
    ExportSink(ObjectPool* pool, const RowDescriptor& row_desc, const std::vector<TExpr>& t_exprs);

    ~ExportSink() override = default;

    Status init(const TDataSink& thrift_sink, RuntimeState* state) override;

    Status prepare(RuntimeState* state) override;

    Status open(RuntimeState* state) override;

    Status send_chunk(RuntimeState* state, Chunk* chunk) override;

    Status close(RuntimeState* state, Status exec_status) override;

    RuntimeProfile* profile() override { return _profile; }

    std::vector<TExpr> get_output_expr() const { return _t_output_expr; }

private:
    Status open_file_writer(int timeout_ms);
    Status gen_file_name(std::string* file_name);

    RuntimeState* _state;

    // owned by RuntimeState
    ObjectPool* _pool;
    const std::vector<TExpr>& _t_output_expr;

    std::vector<ExprContext*> _output_expr_ctxs;

    TExportSink _t_export_sink;

    RuntimeProfile* _profile;

    RuntimeProfile::Counter* _bytes_written_counter;
    RuntimeProfile::Counter* _rows_written_counter;
    RuntimeProfile::Counter* _write_timer;

    std::unique_ptr<FileBuilder> _file_builder;
    bool _closed = false;
};

} // end namespace starrocks
