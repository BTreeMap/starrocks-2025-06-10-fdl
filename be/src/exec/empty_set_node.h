#pragma once

#include "exec/exec_node.h"

namespace starrocks {

/// Node that returns an empty result set, i.e., just sets eos_ in GetNext().
/// Corresponds to EmptySetNode.java in the FE.
class EmptySetNode final : public ExecNode {
public:
    EmptySetNode(ObjectPool* pool, const TPlanNode& tnode, const DescriptorTbl& descs);
    Status get_next(RuntimeState* state, ChunkPtr* chunk, bool* eos) override;

    pipeline::OpFactories decompose_to_pipeline(pipeline::PipelineBuilderContext* context) override;
};

} // namespace starrocks
