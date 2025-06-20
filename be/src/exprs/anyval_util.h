#pragma once

#include "common/status.h"
#include "exprs/expr.h"
#include "exprs/function_context.h"
#include "types/logical_type.h"
#include "util/hash_util.hpp"

namespace starrocks {

class MemPool;

// Utilities for AnyVals
class AnyValUtil {
public:
    static FunctionContext::TypeDesc column_type_to_type_desc(const TypeDescriptor& type);
};

} // namespace starrocks
