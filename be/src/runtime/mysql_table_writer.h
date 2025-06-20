#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "column/binary_column.h"
#include "column/column_viewer.h"
#include "column/vectorized_fwd.h"
#include "common/status.h"
#include "fmt/format.h"
#include "types/logical_type_infra.h"

#ifndef __StarRocksMysql
#define __StarRocksMysql void
#endif

namespace starrocks {

struct MysqlConnInfo {
    std::string host;
    std::string user;
    std::string passwd;
    std::string db;
    int port = 0;

    std::string debug_string() const;
};

class ExprContext;

class MysqlTableWriter {
public:
    using VariantViewer = std::variant<
#define M(NAME) ColumnViewer<NAME>,
            APPLY_FOR_ALL_SCALAR_TYPE(M)
#undef M
                    ColumnViewer<TYPE_NULL>>;

    MysqlTableWriter(const std::vector<ExprContext*>& output_exprs, int chunk_size);
    ~MysqlTableWriter();

    // connnect to mysql server
    Status open(const MysqlConnInfo& conn_info, const std::string& tbl);

    Status begin_trans() { return Status::OK(); }

    Status append(Chunk* chunk);

    Status abort_tarns() { return Status::OK(); }

    Status finish_tarns() { return Status::OK(); }

private:
    Status _build_viewers(Columns& columns);
    Status _build_insert_sql(int from, int to, std::string_view* sql);

    const std::vector<ExprContext*>& _output_expr_ctxs;
    std::vector<VariantViewer> _viewers;

    fmt::memory_buffer _stmt_buffer;
    std::string _escape_buffer;

    std::string _mysql_tbl;
    __StarRocksMysql* _mysql_conn;
    int _chunk_size;
};

} // namespace starrocks
