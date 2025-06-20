#pragma once

#include "exec/exec_node.h"
#include "runtime/mem_pool.h"

namespace starrocks {

// Node that evaluates conjuncts and enforces a limit but otherwise passes along
// the rows pulled from its child unchanged.

class SelectNode final : public ExecNode {
public:
    SelectNode(ObjectPool* pool, const TPlanNode& tnode, const DescriptorTbl& descs);
    ~SelectNode() override;

    Status init(const TPlanNode& tnode, RuntimeState* state) override;
    Status prepare(RuntimeState* state) override;
    Status open(RuntimeState* state) override;
    Status get_next(RuntimeState* state, ChunkPtr* chunk, bool* eos) override;
    void close(RuntimeState* state) override;
    std::vector<std::shared_ptr<pipeline::OperatorFactory>> decompose_to_pipeline(
            pipeline::PipelineBuilderContext* context) override;

private:
    std::map<SlotId, ExprContext*> _common_expr_ctxs;

    RuntimeProfile::Counter* _conjunct_evaluate_timer = nullptr;
};

} // namespace starrocks
