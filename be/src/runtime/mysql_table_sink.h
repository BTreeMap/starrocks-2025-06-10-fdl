#pragma once

#include <memory>
#include <vector>

#include "common/status.h"
#include "exec/data_sink.h"
#include "runtime/mysql_table_writer.h"

namespace starrocks {

class RowDescriptor;
class TExpr;
class TMysqlTableSink;
class RuntimeState;
class RuntimeProfile;
class ExprContext;

// This class is a sinker, which put input data to mysql table
class MysqlTableSink : public DataSink {
public:
    MysqlTableSink(ObjectPool* pool, const RowDescriptor& row_desc, const std::vector<TExpr>& t_exprs);

    ~MysqlTableSink() override;

    Status init(const TDataSink& thrift_sink, RuntimeState* state) override;

    Status prepare(RuntimeState* state) override;

    Status open(RuntimeState* state) override;

    Status send_chunk(RuntimeState* state, Chunk* chunk) override;

    // Flush all buffered data and close all existing channels to destination
    // hosts. Further send() calls are illegal after calling close().
    Status close(RuntimeState* state, Status exec_status) override;

    RuntimeProfile* profile() override { return _profile; }

    std::vector<TExpr> get_output_expr() const { return _t_output_expr; }

private:
    ObjectPool* _pool;
    const std::vector<TExpr>& _t_output_expr;
    int _chunk_size;

    std::vector<ExprContext*> _output_expr_ctxs;

    MysqlConnInfo _conn_info;
    std::string _mysql_tbl;

    std::unique_ptr<MysqlTableWriter> _writer;
    RuntimeProfile* _profile = nullptr;
};

} // namespace starrocks
