#include "testutil/function_utils.h"

#include <vector>

#include "exprs/function_context.h"
#include "runtime/mem_pool.h"

namespace starrocks {

FunctionUtils::FunctionUtils() {
    FunctionContext::TypeDesc return_type;
    std::vector<FunctionContext::TypeDesc> arg_types;
    _memory_pool = new MemPool();
    _fn_ctx = FunctionContext::create_context(_state, _memory_pool, return_type, arg_types);
}
FunctionUtils::FunctionUtils(RuntimeState* state) {
    _state = state;
    FunctionContext::TypeDesc return_type;
    std::vector<FunctionContext::TypeDesc> arg_types;
    _memory_pool = new MemPool();
    _fn_ctx = FunctionContext::create_context(_state, _memory_pool, return_type, arg_types);
}

FunctionUtils::~FunctionUtils() {
    delete _fn_ctx;
    delete _memory_pool;
}

} // namespace starrocks
