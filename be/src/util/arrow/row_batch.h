#pragma once

#include <memory>

#include "common/status.h"
#include "exprs/expr.h"

// This file will convert StarRocks RowBatch to/from Arrow's RecordBatch
// RowBatch is used by StarRocks query engine to exchange data between
// each execute node.

namespace arrow {
class DataType;
class RecordBatch;
class Schema;
class Field;

} // namespace arrow

namespace starrocks {

class RowDescriptor;

Status convert_to_arrow_type(const TypeDescriptor& type, std::shared_ptr<arrow::DataType>* result);
Status convert_to_arrow_field(const TypeDescriptor& desc, const std::string& col_name, bool is_nullable,
                              std::shared_ptr<arrow::Field>* field);

// Convert StarRocks RowDescriptor to Arrow Schema.
Status convert_to_arrow_schema(const RowDescriptor& row_desc,
                               const std::unordered_map<int64_t, std::string>& id_to_col_name,
                               std::shared_ptr<arrow::Schema>* result,
                               const std::vector<ExprContext*>& output_expr_ctxs);

Status serialize_record_batch(const arrow::RecordBatch& record_batch, std::string* result);

Status serialize_arrow_schema(std::shared_ptr<arrow::Schema>* schema, std::string* result);
} // namespace starrocks
