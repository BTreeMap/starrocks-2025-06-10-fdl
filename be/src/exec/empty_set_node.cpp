#include "exec/empty_set_node.h"

#include "exec/pipeline/empty_set_operator.h"
#include "exec/pipeline/pipeline_builder.h"

namespace starrocks {

EmptySetNode::EmptySetNode(ObjectPool* pool, const TPlanNode& tnode, const DescriptorTbl& descs)
        : ExecNode(pool, tnode, descs) {}

Status EmptySetNode::get_next(RuntimeState* state, ChunkPtr* chunk, bool* eos) {
    *eos = true;
    return Status::OK();
}

pipeline::OpFactories EmptySetNode::decompose_to_pipeline(pipeline::PipelineBuilderContext* context) {
    using namespace pipeline;

    return OpFactories{std::make_shared<EmptySetOperatorFactory>(context->next_operator_id(), id())};
}

} // namespace starrocks
