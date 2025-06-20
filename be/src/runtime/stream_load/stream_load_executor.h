#pragma once

namespace starrocks {

class ExecEnv;
class Status;
class StreamLoadContext;
class TTxnCommitAttachment;

class StreamLoadExecutor {
public:
    StreamLoadExecutor(ExecEnv* exec_env) : _exec_env(exec_env) {}

    Status begin_txn(StreamLoadContext* ctx);

    Status commit_txn(StreamLoadContext* ctx);

    Status prepare_txn(StreamLoadContext* ctx);

    Status rollback_txn(StreamLoadContext* ctx);

    Status execute_plan_fragment(StreamLoadContext* ctx);

private:
    // collect the load statistics from context and set them to stat
    // return true if stat is set, otherwise, return false
    bool collect_load_stat(StreamLoadContext* ctx, TTxnCommitAttachment* attachment);

private:
    ExecEnv* _exec_env;
};

} // namespace starrocks
