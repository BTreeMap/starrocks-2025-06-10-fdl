#pragma once

#include <memory>

#include "column/datum.h"
#include "exprs/expr_context.h"
#include "gen_cpp/Exprs_types.h"
#include "types/bitmap_value.h"
#include "types/hll.h"
#include "util/json.h"
#include "util/percentile_value.h"

namespace starrocks {

struct ColumnMapping {
    // <0: use default value
    // >=0: use origin column
    int32_t ref_column{-1};

    // materialized view function.
    ExprContext* mv_expr_ctx;

    // the following data is used by default_value_datum, because default_value_datum only
    // have the reference. We need to keep the content has the same life cycle as the
    // default_value_datum;
    std::unique_ptr<HyperLogLog> default_hll;
    std::unique_ptr<BitmapValue> default_bitmap;
    std::unique_ptr<PercentileValue> default_percentile;
    std::unique_ptr<JsonValue> default_json;

    Datum default_value_datum;
};

typedef std::vector<ColumnMapping> SchemaMapping;

} // namespace starrocks
