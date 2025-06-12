# SQL Distributed Joins Backend Architecture in StarRocks

## Overview

This document provides a comprehensive overview of how SQL distributed joins work in the StarRocks backend engine. It covers the entire processing flow from query entrypoint to execution completion, explaining the architecture, components, and algorithms involved in distributed join operations. **Every function call hierarchy and interaction is documented with links to the actual source files in the codebase.**

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Join Distribution Strategies](#join-distribution-strategies)
3. [Component Architecture](#component-architecture)
4. [Execution Flow](#execution-flow)
5. [Complete Function Call Hierarchy](#complete-function-call-hierarchy)
6. [Join Algorithms Implementation](#join-algorithms-implementation)
7. [Runtime Filter System](#runtime-filter-system)
8. [Pipeline Execution Engine](#pipeline-execution-engine)
9. [Performance Optimization](#performance-optimization)

## Architecture Overview

StarRocks implements a distributed query execution engine based on a **pipeline architecture** with vectorized processing. The join execution involves multiple components working together:

```
Query Planning → Fragment Distribution → Join Execution → Result Collection
      ↓              ↓                    ↓               ↓
  Cost-Based    Exchange Operators    Hash Join Ops   Result Merging
  Optimizer      Data Shuffling       Build/Probe     Pipeline Fusion
```

### Key Design Principles

1. **Vectorized Processing**: All join operations process data in chunks (typically 4096 rows)
2. **Pipeline Parallelism**: Build and probe phases run in parallel across multiple threads
3. **Memory Efficiency**: Spill-to-disk support for large joins
4. **Runtime Optimization**: Dynamic runtime filters to reduce data movement

## Join Distribution Strategies

StarRocks supports four main distributed join strategies:

### 1. Broadcast Join

**When Used**: Small table (build side) joins with large table (probe side)

**How It Works**:
- Build table data is replicated to all BE nodes
- Each BE performs local join with its portion of probe table
- No data shuffling required for probe table

**Key Files**:
- `TJoinDistributionMode::BROADCAST`
- `ExchangeSinkOperator` with `TPartitionType::UNPARTITIONED`

```cpp
// Distribution mode determination in HashJoinNode
if (_distribution_mode == TJoinDistributionMode::BROADCAST) {
    // Build side replicated to all nodes
    // Probe side remains distributed
}
```

### 2. Shuffle Join (Hash Partitioned)

**When Used**: Both tables are large and need to be redistributed

**How It Works**:
- Both tables are hash-partitioned on join keys
- Data with same hash values sent to same BE node
- Local join performed on each node

**Key Files**:
- `TPartitionType::HASH_PARTITIONED`
- `DataStreamSender` for data redistribution

### 3. Bucket Shuffle Join

**When Used**: One table has bucketing key matching join key

**How It Works**:
- Table without matching bucketing key is shuffled to match distribution
- Leverages existing data distribution to minimize shuffling
- Only one table needs redistribution

**Key Files**:
- `TPartitionType::BUCKET_SHUFFLE_HASH_PARTITIONED`
- `TJoinDistributionMode::LOCAL_HASH_BUCKET`

### 4. Colocate Join

**When Used**: Both tables belong to same colocation group

**How It Works**:
- No data movement required
- Join performed locally on each BE node
- Highest performance for matching colocation groups

**Key Files**:
- `TJoinDistributionMode::COLOCATE`
- Colocation group management

## Component Architecture

### Core Join Components

#### 1. HashJoinNode
**Location**: `be/src/exec/hash_join_node.h/cpp`

Primary join execution node that orchestrates the entire join process:

```cpp
class HashJoinNode final : public ExecNode {
private:
    TJoinDistributionMode::type _distribution_mode;
    JoinHashTable _ht;                    // Hash table for build phase
    std::vector<ExprContext*> _build_expr_ctxs;  // Build key expressions
    std::vector<ExprContext*> _probe_expr_ctxs;  // Probe key expressions
    std::list<RuntimeFilterBuildDescriptor*> _build_runtime_filters;
};
```

**Key Responsibilities**:
- Initialize hash table parameters
- Coordinate build and probe phases
- Generate runtime filters
- Handle different join types (INNER, LEFT, RIGHT, FULL, SEMI, ANTI)

#### 2. HashJoiner
**Location**: `be/src/exec/hash_joiner.h`

Core join algorithm implementation:

```cpp
struct HashJoinerParam {
    const THashJoinNode& _hash_join_node;
    std::vector<ExprContext*> _build_expr_ctxs;
    std::vector<ExprContext*> _probe_expr_ctxs;
    TJoinDistributionMode::type _distribution_mode;
    bool _enable_late_materialization;
    bool _enable_partition_hash_join;
};

enum HashJoinPhase {
    BUILD = 0,      // Building hash table from right child
    PROBE = 1,      // Probing hash table with left child
    POST_PROBE = 2, // Processing unmatched tuples
    EOS = 4         // End of stream
};
```

#### 3. JoinHashMap
**Location**: `be/src/exec/join_hash_map.h/cpp`

Optimized hash table implementation with multiple specializations:

```cpp
// Different hash map implementations based on key types
class JoinHashMapForOneKey;           // Single key optimization
class JoinHashMapForFixedSize;        // Fixed-size keys
class JoinHashMapForSerialized;       // Variable-length keys
```

**Optimizations**:
- Direct mapping for single integer keys
- Fixed-size key optimization for multiple primitive keys
- Serialized keys for complex/variable-length data

### Exchange System

#### 1. ExchangeNode
**Location**: `be/src/exec/exchange_node.h/cpp`

Receives data from remote fragments:

```cpp
class ExchangeNode final : public ExecNode {
private:
    std::shared_ptr<DataStreamRecvr> _stream_recvr;
    bool _is_merging;  // Whether to merge sorted streams
    int _num_senders;  // Number of sending fragments
};
```

#### 2. DataStreamSender
**Location**: `be/src/runtime/data_stream_sender.h/cpp`

Sends data to remote fragments with partitioning logic:

```cpp
class DataStreamSender {
private:
    TPartitionType::type _part_type;
    std::vector<ExprContext*> _partition_expr_ctxs;
    std::vector<Channel> _channels;  // One per destination
};
```

**Partitioning Types**:
- `UNPARTITIONED`: Broadcast to all destinations
- `HASH_PARTITIONED`: Hash-based partitioning
- `BUCKET_SHUFFLE_HASH_PARTITIONED`: Bucket-aware partitioning
- `RANDOM`: Random distribution

## Complete Function Call Hierarchy

This section documents **all functions called during distributed join execution** with their **complete interaction flow** and **links to source files**.

### Function Call List
Below is the comprehensive list of core functions invoked during a distributed join execution, with direct links to their source files:
- [FragmentMgr::exec_plan_fragment()](be/src/runtime/fragment_mgr.h) → Implements fragment dispatch and lifecycle (see `fragment_mgr.cpp` for definition)
- [FragmentContext::prepare_all_pipelines()](be/src/exec/pipeline/fragment_context.h) → Prepares and instantiates pipelines within a fragment
- [PipelineDriverExecutor::submit()](be/src/exec/pipeline/pipeline_driver_executor.h) → Enqueues drivers for execution in the driver executor
- [PipelineDriver::process()](be/src/exec/pipeline/pipeline_driver.h) → Main execution loop for pipeline drivers

### 1. Query Entry Point and Fragment Execution

#### **FragmentMgr::exec_plan_fragment()**
**Location**: `be/src/runtime/fragment_mgr.h/cpp`

Entry point for executing query fragments containing joins:

```cpp
Status FragmentMgr::exec_plan_fragment(const TExecPlanFragmentParams& params) {
    // 1. Create fragment instance
    // 2. Register with fragment context manager
    // 3. Submit to thread pool for execution
    
    // Key function calls:
    fragment_ctx = query_ctx->fragment_mgr()->get_or_register(fragment_id);
    pipeline_executor->submit(fragment_ctx);
}
```

**Called by**: Frontend query coordinator  
**Calls**: [`FragmentContext::prepare_all_pipelines()`](#fragmentcontextprepare_all_pipelines), [`PipelineDriverExecutor::submit()`](#pipelinedriverexecutorsubmit)

#### **FragmentContext::prepare_all_pipelines()**
**Location**: `be/src/exec/pipeline/fragment_context.h/cpp`

Prepares all pipelines within a fragment for execution:

```cpp
Status FragmentContext::prepare_all_pipelines() {
    for (auto& pipeline : _pipelines) {
        RETURN_IF_ERROR(pipeline->prepare(_runtime_state));
        // Instantiate drivers for parallel execution
        pipeline->instantiate_drivers(_runtime_state);
    }
}
```

**Called by**: [`FragmentMgr::exec_plan_fragment()`](#fragmentmgrexec_plan_fragment)  
**Calls**: [`Pipeline::prepare()`](#pipelineprepare), [`Pipeline::instantiate_drivers()`](#pipelineinstantiate_drivers)

### 2. Pipeline Driver Execution

#### **PipelineDriverExecutor::submit()**
**Location**: `be/src/exec/pipeline/pipeline_driver_executor.h/cpp`

Submits pipeline drivers to execution queues:

```cpp
void PipelineDriverExecutor::submit(DriverPtr driver) {
    // Determine execution queue based on driver properties
    // Submit to appropriate workgroup queue
    _driver_queue->put_back(driver.get());
}
```

**Called by**: [`FragmentContext::prepare_all_pipelines()`](#fragmentcontextprepare_all_pipelines)  
**Calls**: [`DriverQueue::put_back()`](#driverqueueput_back), [`PipelineDriver::process()`](#pipelinedriverprocess)

#### **PipelineDriver::process()**
**Location**: `be/src/exec/pipeline/pipeline_driver.h/cpp`

Main driver execution loop:

```cpp
StatusOr<DriverState> PipelineDriver::process(RuntimeState* state, int worker_id) {
    while (!is_finished()) {
        // Process operators in pipeline
        for (auto& op : _operators) {
            if (op->need_input()) {
                RETURN_IF_ERROR(op->push_chunk(state, chunk));
            }
            if (op->has_output()) {
                auto chunk = op->pull_chunk(state);
                // Pass to next operator
            }
        }
    }
}
```

**Called by**: [`PipelineDriverExecutor::submit()`](#pipelinedriverexecutorsubmit)  
**Calls**: [`Operator::push_chunk()`](#operatorpush_chunk), [`Operator::pull_chunk()`](#operatorpull_chunk)

### 3. HashJoin Build Phase Functions

#### **HashJoinBuildOperator::push_chunk()**
**Location**: `be/src/exec/pipeline/hashjoin/hash_join_build_operator.h/cpp`

Processes build-side chunks:

```cpp
Status HashJoinBuildOperator::push_chunk(RuntimeState* state, const ChunkPtr& chunk) {
    return _join_builder->append_chunk_to_ht(chunk);
}
```

**Called by**: [`PipelineDriver::process()`](#pipelinedriverprocess)  
**Calls**: [`HashJoiner::append_chunk_to_ht()`](#hashjoinerappend_chunk_to_ht)

#### **HashJoiner::append_chunk_to_ht()**
**Location**: `be/src/exec/hash_joiner.h/cpp`

Appends chunks to hash table:

```cpp
Status HashJoiner::append_chunk_to_ht(const ChunkPtr& chunk) {
    if (_phase != HashJoinPhase::BUILD) return Status::OK();
    if (!chunk || chunk->is_empty()) return Status::OK();
    
    update_build_rows(chunk->num_rows());
    return _hash_join_builder->append_chunk(chunk);
}
```

**Called by**: [`HashJoinBuildOperator::push_chunk()`](#hashjoinbuilderoperatorpush_chunk)  
**Calls**: [`HashJoinBuilder::append_chunk()`](#hashjoinbuilderappend_chunk)

#### **HashJoinBuilder::append_chunk()**
**Location**: `be/src/exec/hash_join_components.h/cpp`

Core hash table building logic:

```cpp
Status HashJoinBuilder::append_chunk(const ChunkPtr& chunk) {
    // Evaluate join key expressions
    RETURN_IF_ERROR(_evaluate_join_keys(chunk, &key_columns));
    
    // Build hash table entries
    RETURN_IF_ERROR(_hash_table.build(key_columns, chunk));
    
    // Update metrics
    _build_rows += chunk->num_rows();
}
```

**Called by**: [`HashJoiner::append_chunk_to_ht()`](#hashjoinerappend_chunk_to_ht)  
**Calls**: [`HashJoinBuilder::_evaluate_join_keys()`](#hashjoinbuilder_evaluate_join_keys), [`JoinHashTable::build()`](#joinhashtablebuild)

#### **HashJoinBuildOperator::set_finishing()**
**Location**: `be/src/exec/pipeline/hashjoin/hash_join_build_operator.h/cpp`

Completes hash table construction:

```cpp
Status HashJoinBuildOperator::set_finishing(RuntimeState* state) {
    // Build hash table from accumulated chunks
    RETURN_IF_ERROR(_join_builder->build_ht(state));
    
    // Create runtime filters
    RETURN_IF_ERROR(_join_builder->create_runtime_filters(state));
    
    // Notify probe operators
    _join_builder->defer_notify_probe();
}
```

**Called by**: [`PipelineDriver::process()`](#pipelinedriverprocess)  
**Calls**: [`HashJoiner::build_ht()`](#hashjoinerbuild_ht), [`HashJoiner::create_runtime_filters()`](#hashjoinercreate_runtime_filters)

#### **HashJoiner::build_ht()**
**Location**: `be/src/exec/hash_joiner.h/cpp`

Finalizes hash table construction:

```cpp
Status HashJoiner::build_ht(RuntimeState* state) {
    // Complete hash table building
    RETURN_IF_ERROR(_hash_join_builder->build(state));
    
    // Set build phase complete
    _phase = HashJoinPhase::PROBE;
    
    // Update metrics
    _build_metrics->hash_table_memory_usage->set(_hash_join_builder->ht_mem_usage());
}
```

**Called by**: [`HashJoinBuildOperator::set_finishing()`](#hashjoinbuilderoperatorset_finishing)  
**Calls**: [`HashJoinBuilder::build()`](#hashjoinbuilderbuild)

### 4. HashJoin Probe Phase Functions

#### **HashJoinProbeOperator::push_chunk()**
**Location**: `be/src/exec/pipeline/hashjoin/hash_join_probe_operator.h/cpp`

Processes probe-side chunks:

```cpp
Status HashJoinProbeOperator::push_chunk(RuntimeState* state, const ChunkPtr& chunk) {
    RETURN_IF_ERROR(_reference_builder_hash_table_once());
    RETURN_IF_ERROR(_join_prober->push_chunk(state, std::move(const_cast<ChunkPtr&>(chunk))));
    return Status::OK();
}
```

**Called by**: [`PipelineDriver::process()`](#pipelinedriverprocess)  
**Calls**: [`HashJoinProbeOperator::_reference_builder_hash_table_once()`](#hashjoinprobeoperator_reference_builder_hash_table_once), [`HashJoiner::push_chunk()`](#hashjoinerpush_chunk)

#### **HashJoinProbeOperator::_reference_builder_hash_table_once()**
**Location**: `be/src/exec/pipeline/hashjoin/hash_join_probe_operator.h/cpp`

References the built hash table:

```cpp
Status HashJoinProbeOperator::_reference_builder_hash_table_once() {
    if (!is_ready() || _join_prober->has_referenced_hash_table()) {
        return Status::OK();
    }
    _join_prober->reference_hash_table(_join_builder.get());
    return Status::OK();
}
```

**Called by**: [`HashJoinProbeOperator::push_chunk()`](#hashjoinprobeoperatorpush_chunk)  
**Calls**: [`HashJoiner::reference_hash_table()`](#hashjoinerreference_hash_table)

#### **HashJoinProbeOperator::pull_chunk()**
**Location**: `be/src/exec/pipeline/hashjoin/hash_join_probe_operator.h/cpp`

Retrieves joined results:

```cpp
StatusOr<ChunkPtr> HashJoinProbeOperator::pull_chunk(RuntimeState* state) {
    RETURN_IF_ERROR(_reference_builder_hash_table_once());
    return _join_prober->pull_chunk(state);
}
```

**Called by**: [`PipelineDriver::process()`](#pipelinedriverprocess)  
**Calls**: [`HashJoiner::pull_chunk()`](#hashjoinerpull_chunk)

#### **HashJoiner::push_chunk()** / **HashJoiner::pull_chunk()**
**Location**: `be/src/exec/hash_joiner.h/cpp`

Core probe operations:

```cpp
Status HashJoiner::push_chunk(RuntimeState* state, ChunkPtr chunk) {
    return _hash_join_prober->push_chunk(state, std::move(chunk));
}

StatusOr<ChunkPtr> HashJoiner::pull_chunk(RuntimeState* state) {
    return _hash_join_prober->pull_chunk(state);
}
```

**Called by**: [`HashJoinProbeOperator::push_chunk()`](#hashjoinprobeoperatorpush_chunk), [`HashJoinProbeOperator::pull_chunk()`](#hashjoinprobeoperatorpull_chunk)  
**Calls**: [`HashJoinProber::push_chunk()`](#hashjoinproberpush_chunk), [`HashJoinProber::pull_chunk()`](#hashjoinproberpull_chunk)

### 5. Hash Table Core Operations

#### **JoinHashTable::build()**
**Location**: `be/src/exec/join_hash_map.h/cpp`

Hash table construction with optimized implementations:

```cpp
template<LogicalType TYPE>
Status JoinHashMapForOneKey<TYPE>::build(const Columns& key_columns, const ChunkPtr& chunk) {
    // Direct key access for single-column joins
    auto key_column = key_columns[0];
    for (size_t i = 0; i < chunk->num_rows(); ++i) {
        auto key = key_column->get(i).get<TYPE>();
        _hash_map[key].push_back(i + _build_row_offset);
    }
    _build_row_offset += chunk->num_rows();
}

Status JoinHashMapForSerialized::build(const Columns& key_columns, const ChunkPtr& chunk) {
    // Serialize complex keys for multi-column joins
    std::vector<uint32_t> hash_values;
    _serialize_keys(key_columns, &hash_values);
    
    for (size_t i = 0; i < chunk->num_rows(); ++i) {
        uint32_t hash_val = hash_values[i];
        _hash_table.emplace(hash_val, i + _build_row_offset);
    }
    _build_row_offset += chunk->num_rows();
}
```

**Called by**: [`HashJoinBuilder::append_chunk()`](#hashjoinbuilderappend_chunk)  
**Calls**: Various specialized hash map implementations based on key types

#### **JoinHashTable::probe()**
**Location**: `be/src/exec/join_hash_map.h/cpp`

Hash table probing with join type specializations:

```cpp
template<LogicalType TYPE>
void JoinHashMapForOneKey<TYPE>::probe(const Columns& key_columns, 
                                       JoinHashTableItems* result) {
    auto key_column = key_columns[0];
    for (size_t i = 0; i < key_column->size(); ++i) {
        auto key = key_column->get(i).get<TYPE>();
        auto it = _hash_map.find(key);
        if (it != _hash_map.end()) {
            for (auto build_row : it->second) {
                result->add_match(i, build_row);
            }
        }
    }
}

// Specialized probe functions for different join types:
void probe_inner_join(const Columns& probe_keys, JoinHashTableItems* result);
void probe_left_outer_join(const Columns& probe_keys, JoinHashTableItems* result);
void probe_semi_join(const Columns& probe_keys, JoinHashTableItems* result);
void probe_anti_join(const Columns& probe_keys, JoinHashTableItems* result);
```

**Called by**: [`HashJoinProber::pull_chunk()`](#hashjoinproberpull_chunk)  
**Calls**: Type-specific probe implementations

### 6. Data Distribution Functions

#### **DataStreamSender::send()**
**Location**: `be/src/runtime/data_stream_sender.h/cpp`

Distributes data to remote fragments:

```cpp
Status DataStreamSender::send(RuntimeState* state, const ChunkPtr& chunk) {
    if (_part_type == TPartitionType::HASH_PARTITIONED) {
        return _send_hash_partitioned(state, chunk);
    } else if (_part_type == TPartitionType::UNPARTITIONED) {
        return _send_broadcast(state, chunk);
    }
    // Other partition types...
}
```

**Called by**: Exchange sink operators  
**Calls**: [`DataStreamSender::_send_hash_partitioned()`](#datastreamsender_send_hash_partitioned), [`DataStreamSender::_send_broadcast()`](#datastreamsender_send_broadcast)

#### **ExchangeNode::get_next()**
**Location**: `be/src/exec/exchange_node.h/cpp`

Receives data from remote fragments:

```cpp
Status ExchangeNode::get_next(RuntimeState* state, ChunkPtr* chunk, bool* eos) {
    return _stream_recvr->get_next(chunk, eos);
}
```

**Called by**: Pipeline operators  
**Calls**: [`DataStreamRecvr::get_next()`](#datastreamrecvrget_next)

### 7. Runtime Filter Functions

#### **RuntimeFilterBuilder::build_filters()**
**Location**: `be/src/exprs/runtime_filter_bank.h/cpp`

Creates runtime filters from join build side:

```cpp
Status RuntimeFilterBuilder::build_filters(RuntimeState* state, const ChunkPtr& chunk) {
    for (auto& rf_desc : _build_runtime_filters) {
        // Extract filter keys from chunk
        Columns filter_columns;
        RETURN_IF_ERROR(_evaluate_filter_keys(chunk, &filter_columns));
        
        // Build bloom filter or min-max filter
        RETURN_IF_ERROR(rf_desc->build_filter(filter_columns));
    }
}
```

**Called by**: [`HashJoiner::create_runtime_filters()`](#hashjoinercreate_runtime_filters)  
**Calls**: [`RuntimeFilterBuildDescriptor::build_filter()`](#runtimefilterbuildescriptorbuild_filter)

#### **RuntimeFilterProbeCollector::evaluate()**
**Location**: `be/src/exprs/runtime_filter_bank.h/cpp`

Applies runtime filters during scan:

```cpp
void RuntimeFilterProbeCollector::evaluate(Chunk* chunk, Filter* selection) {
    for (auto& rf_desc : _probe_runtime_filters) {
        // Apply runtime filter to chunk
        rf_desc->evaluate_filter(chunk, selection);
    }
}
```

**Called by**: Scan operators  
**Calls**: [`RuntimeFilterProbeDescriptor::evaluate_filter()`](#runtimefilterprobedescriptorevaluate_filter)

### 8. Function Call Flow Summary

The complete execution flow follows this pattern:

```
Query Entry:
FragmentMgr::exec_plan_fragment()
└── FragmentContext::prepare_all_pipelines()
    └── Pipeline::prepare()
        └── Pipeline::instantiate_drivers()
            └── PipelineDriverExecutor::submit()

Driver Execution:
PipelineDriverExecutor::submit()
└── PipelineDriver::process()
    └── [For each operator in pipeline]
        ├── Operator::push_chunk()
        └── Operator::pull_chunk()

Join Build Phase:
HashJoinBuildOperator::push_chunk()
└── HashJoiner::append_chunk_to_ht()
    └── HashJoinBuilder::append_chunk()
        └── JoinHashTable::build()

Build Finalization:
HashJoinBuildOperator::set_finishing()
└── HashJoiner::build_ht()
    ├── HashJoinBuilder::build()
    └── HashJoiner::create_runtime_filters()
        └── RuntimeFilterBuilder::build_filters()

Join Probe Phase:
HashJoinProbeOperator::push_chunk()
└── HashJoiner::push_chunk()
    └── HashJoinProber::push_chunk()

HashJoinProbeOperator::pull_chunk()
└── HashJoiner::pull_chunk()
    └── HashJoinProber::pull_chunk()
        └── JoinHashTable::probe()

Data Distribution:
DataStreamSender::send()
├── _send_hash_partitioned() [for shuffle joins]
├── _send_broadcast() [for broadcast joins]
└── _send_bucket_shuffle() [for bucket shuffle joins]

ExchangeNode::get_next()
└── DataStreamRecvr::get_next()
```

**Key Interaction Points**:

1. **Pipeline Driver Orchestration**: [`PipelineDriverExecutor`] manages execution scheduling
2. **Join Coordination**: [`HashJoiner`] coordinates between build and probe operators
3. **Hash Table Operations**: [`JoinHashTable`] implementations handle type-specific optimizations
4. **Data Movement**: [`DataStreamSender`]/[`ExchangeNode`] handle distributed data transfer
5. **Runtime Optimization**: [`RuntimeFilterBuilder`]/[`RuntimeFilterProbeCollector`] provide dynamic filtering

### 3. Build Phase

For hash joins, the build phase:

1. **Scan Build Table**: Right child scanned in parallel
2. **Apply Predicates**: Local filters applied early
3. **Build Hash Table**: Keys hashed and inserted
4. **Generate Runtime Filters**: Bloom filters created for probe side
5. **Broadcast Filters**: Runtime filters sent to probe fragments

```cpp
// Simplified build phase flow
Status HashJoinNode::_build(RuntimeState* state) {
    // 1. Scan right child (build side)
    while (!build_eos) {
        RETURN_IF_ERROR(child(1)->get_next(state, &chunk, &build_eos));
        
        // 2. Evaluate build keys
        RETURN_IF_ERROR(_evaluate_build_keys(chunk));
        
        // 3. Insert into hash table
        RETURN_IF_ERROR(_ht.append_chunk(chunk));
    }
    
    // 4. Finalize hash table
    RETURN_IF_ERROR(_ht.build(state));
    
    // 5. Create runtime filters
    RETURN_IF_ERROR(_create_runtime_filters(state));
}
```

### 4. Probe Phase

The probe phase processes left child data:

1. **Scan Probe Table**: Left child scanned with runtime filters
2. **Hash and Lookup**: Keys hashed and looked up in build table
3. **Join Logic**: Different logic per join type
4. **Output Generation**: Result tuples constructed

```cpp
Status HashJoinNode::_probe(RuntimeState* state, ChunkPtr* chunk, bool& eos) {
    // 1. Get probe chunk
    RETURN_IF_ERROR(child(0)->get_next(state, &probe_chunk, &probe_eos));
    
    // 2. Apply runtime filters
    RETURN_IF_ERROR(_apply_runtime_filters(probe_chunk));
    
    // 3. Evaluate probe keys
    RETURN_IF_ERROR(_evaluate_probe_keys(probe_chunk));
    
    // 4. Probe hash table
    RETURN_IF_ERROR(_ht.probe(probe_chunk, result_chunk));
    
    // 5. Apply other join conjuncts
    RETURN_IF_ERROR(_process_other_conjunct(result_chunk));
}
```

## Join Algorithms Implementation

### Hash Join Core Algorithm

StarRocks uses a **hash-based join algorithm** with the following optimizations:

#### 1. Key Hashing Strategies

```cpp
// Single key optimization
template<LogicalType TYPE>
class JoinHashMapForOneKey {
    // Direct mapping for integer types
    // CRC32 hash for others
};

// Multi-key optimization  
template<LogicalType TYPE>
class JoinHashMapForFixedSize {
    // Combine multiple keys into single hash
    // Memory-efficient storage
};

// Variable-length keys
class JoinHashMapForSerialized {
    // Serialize complex keys
    // Handle nulls and variable types
};
```

#### 2. Hash Table Structure

```cpp
struct JoinHashTableItems {
    std::vector<uint32_t> first;        // Hash buckets
    std::vector<uint32_t> next;         // Collision chains
    Columns key_columns;                // Build key storage
    TJoinOp::type join_type;           // Join type
    bool with_other_conjunct;          // Non-equi predicates
};
```

#### 3. Probe Algorithms

Different probe strategies based on join type:

```cpp
// Inner join: only output matches
void probe_inner_join(probe_chunk, &result);

// Left outer join: output all probe rows, null-fill non-matches  
void probe_left_outer_join(probe_chunk, &result);

// Semi join: output probe rows that have matches
void probe_semi_join(probe_chunk, &result);

// Anti join: output probe rows that have no matches
void probe_anti_join(probe_chunk, &result);
```

### SIMD Optimizations

StarRocks leverages SIMD instructions for:
- Hash computation (CRC32 with SSE4.2)
- Batch key comparison
- Null value handling
- Filter evaluation

## Runtime Filter System

Runtime filters are a key optimization for distributed joins:

### 1. Filter Types

```cpp
// Bloom filters for high-cardinality keys
class ComposedRuntimeBloomFilter<TYPE>;

// Min-max filters for range pruning
class MinMaxRuntimeFilter<TYPE>;

// IN filters for low-cardinality keys  
class ComposedRuntimeInFilter<TYPE>;
```

### 2. Filter Generation

During build phase:

```cpp
class RuntimeFilterBuildDescriptor {
    int32_t filter_id;
    TRuntimeFilterBuildJoinMode::type build_join_mode;
    std::vector<ExprContext*> build_expr_ctxs;
};
```

### 3. Filter Distribution

Filters are distributed based on join mode:

- **Broadcast Join**: Filters sent to all probe fragments
- **Shuffle Join**: Filters partitioned by hash
- **Bucket Shuffle**: Filters follow bucket distribution
- **Colocate Join**: Filters stay local

### 4. Filter Application

At scan operators:

```cpp
// Apply runtime filters during scan
class RuntimeFilterProbeCollector {
    void evaluate(Chunk* chunk, Filter* selection);
    void add_descriptor(RuntimeFilterProbeDescriptor* desc);
};
```

## Pipeline Execution Engine

StarRocks uses a pipeline execution model where operations are decomposed into operators:

### 1. Pipeline Decomposition

```cpp
// HashJoinNode decomposes into:
pipeline::OpFactories HashJoinNode::decompose_to_pipeline(context) {
    // Build pipeline: Scan → HashJoinBuild
    // Probe pipeline: Scan → HashJoinProbe → Project → ...
}
```

### 2. Pipeline Operators

#### HashJoinBuildOperator
**Location**: `be/src/exec/pipeline/hashjoin/hash_join_build_operator.h`

```cpp
class HashJoinBuildOperator final : public Operator {
    Status push_chunk(RuntimeState* state, const ChunkPtr& chunk) override;
    bool is_finished() const override;
    Status set_finishing(RuntimeState* state) override;
private:
    HashJoinerPtr _join_builder;
    TJoinDistributionMode::type _distribution_mode;
};
```

#### HashJoinProbeOperator  
**Location**: `be/src/exec/pipeline/hashjoin/hash_join_probe_operator.h`

```cpp
class HashJoinProbeOperator final : public SourceOperator {
    StatusOr<ChunkPtr> pull_chunk(RuntimeState* state) override;
    bool has_output() const override;
    bool is_finished() const override;
private:
    HashJoinerPtr _join_prober;
    ChunkPtr _curr_left_chunk;
};
```

### 3. Parallel Execution

The pipeline engine provides:
- **Intra-operator parallelism**: Multiple threads per operator
- **Inter-operator parallelism**: Pipeline stages run concurrently  
- **NUMA awareness**: Memory and thread locality
- **Adaptive scheduling**: Dynamic work stealing

## Performance Optimization

### 1. Late Materialization

For wide tables, only join keys are processed initially:

```cpp
bool _enable_late_materialization;

// Phase 1: Join on keys only
// Phase 2: Materialize other columns for matches
```

### 2. Partition Hash Join

For memory-bounded environments:

```cpp
bool _enable_partition_hash_join;

// Partition both sides by hash
// Process partitions one at a time
// Reduce memory footprint
```

### 3. Spill-to-Disk

When memory is insufficient:

```cpp
class SpillableHashJoinBuildOperator : public HashJoinBuildOperator {
    // Spill hash table partitions to disk
    // Read back during probe phase
    Status spill_partition(size_t partition_id);
    Status restore_partition(size_t partition_id);
};
```

### 4. Skew Handling

For data skew:

```cpp
bool _is_skew_join;

// Detect skewed keys during build
// Use different strategies for skewed vs normal data
// Broadcast very hot keys, shuffle others
```

### 5. Memory Management

```cpp
class ChunkBufferMemoryManager {
    // Track memory usage across operators
    // Apply backpressure when memory is low
    // Coordinate spilling decisions
};
```

## Code Organization

### Key Source Files

**Core Join Logic**:
- `be/src/exec/hash_join_node.h/cpp` - Main join operator
- `be/src/exec/hash_joiner.h/cpp` - Join algorithm implementation  
- `be/src/exec/join_hash_map.h/cpp` - Optimized hash table
- `be/src/exec/hash_join_components.h/cpp` - Supporting utilities

**Pipeline Operators**:
- `be/src/exec/pipeline/hashjoin/hash_join_build_operator.h/cpp`
- `be/src/exec/pipeline/hashjoin/hash_join_probe_operator.h/cpp`
- `be/src/exec/pipeline/hashjoin/hash_joiner_factory.h/cpp`

**Exchange System**:
- `be/src/exec/exchange_node.h/cpp` - Data receiving
- `be/src/runtime/data_stream_sender.h/cpp` - Data sending
- `be/src/exec/pipeline/exchange/` - Pipeline exchange operators

**Runtime Filters**:
- `be/src/exprs/runtime_filter_bank.h/cpp` - Filter management
- `be/src/exec/pipeline/runtime_filter_types.h/cpp` - Pipeline integration

### Test Coverage

**Unit Tests**:
- `be/test/exec/join_hash_map_test.cpp` - Hash table algorithms
- `be/test/exec/hash_join_node_test.cpp` - Join operator tests
- `be/test/exprs/runtime_filter_test.cpp` - Runtime filter tests

**Integration Tests**:
- Pipeline execution tests
- End-to-end query tests  
- Performance regression tests

## Conclusion

StarRocks' distributed join system represents a sophisticated implementation that balances performance, scalability, and resource efficiency. Key strengths include:

1. **Multiple Distribution Strategies**: Optimal strategy selection based on data characteristics
2. **Vectorized Pipeline Execution**: High throughput with low CPU overhead
3. **Advanced Runtime Optimization**: Dynamic filters and adaptive execution
4. **Memory Management**: Spill-to-disk and memory-bounded execution
5. **SIMD Optimizations**: Hardware-accelerated operations

The modular architecture allows for continued optimization and feature development while maintaining high performance for diverse workloads.
