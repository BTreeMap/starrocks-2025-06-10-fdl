namespace starrocks {
class FunctionContext;
}

namespace starrocks {

class MemPool;
class RuntimeState;

class FunctionUtils {
public:
    FunctionUtils();
    FunctionUtils(RuntimeState* state);
    ~FunctionUtils();

    FunctionContext* get_fn_ctx() { return _fn_ctx; }

private:
    RuntimeState* _state = nullptr;
    MemPool* _memory_pool = nullptr;
    FunctionContext* _fn_ctx = nullptr;
};

} // namespace starrocks
