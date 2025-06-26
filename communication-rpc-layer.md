# StarRocks Communication/RPC Layer in Distributed Joins: A Deep Dive

## Executive Summary

This report provides a comprehensive analysis of the communication and RPC layer used in StarRocks distributed joins, with particular focus on the Shuffle Join implementation. StarRocks employs a dual-protocol approach: **Apache Thrift for control-plane operations** and **bRPC (Baidu RPC) with Protocol Buffers for high-performance data-plane communication**. This architecture enables efficient distributed query execution while maintaining compatibility and operational simplicity.

## 1. Protocol Architecture Overview

### 1.1 Dual-Protocol Design

StarRocks implements a sophisticated communication stack with clearly separated concerns:

**Control Plane (Apache Thrift):**

- Frontend-Backend (FE-BE) communication
- Fragment lifecycle management
- Metadata operations
- Query planning coordination
- Error reporting and status updates

**Data Plane (bRPC + Protobuf):**

- Chunk transmission during distributed joins
- High-throughput data exchange
- Runtime filter propagation
- Stream processing operations

### 1.2 Key Components

#### DataStreamSender

**Location**: `be/src/runtime/data_stream_sender.h/cpp`

The DataStreamSender is responsible for partitioning and distributing data to remote fragments:

```cpp
class DataStreamSender final : public DataSink {
private:
    TPartitionType::type _part_type;
    std::vector<ExprContext*> _partition_expr_ctxs;
    std::vector<Channel*> _channels;  // One per destination
    std::shared_ptr<PInternalService_RecoverableStub> _brpc_stub;
};
```

**Supported Partitioning Types:**

- `UNPARTITIONED`: Broadcast to all destinations
- `HASH_PARTITIONED`: Hash-based partitioning for shuffle joins
- `BUCKET_SHUFFLE_HASH_PARTITIONED`: Bucket-aware partitioning
- `RANDOM`: Round-robin distribution

#### DataStreamReceiver (DataStreamRecvr)

**Location**: `be/src/runtime/data_stream_recvr.h/cpp`

Manages incoming data streams with sophisticated buffering and flow control:

```cpp
class DataStreamRecvr {
private:
    std::vector<SenderQueue*> _sender_queues;
    std::atomic<size_t> _num_buffered_bytes{0};
    bool _is_merging;  // Whether to merge sorted streams
    PassThroughChunkBuffer* _pass_through_chunk_buffer;
};
```

## 2. Network Protocols in Detail

### 2.1 Apache Thrift Usage

**Configuration Parameters** (from `be/src/common/config.h`):

```cpp
// Thrift server configuration
CONF_Int32(be_port, "9060");  // Main Thrift service port
CONF_Bool(thrift_rpc_strict_mode, "true");
CONF_Int32(thrift_rpc_max_body_size, "2147483648");
CONF_Int32(thrift_client_retry_interval_ms, "100");
CONF_Int32(thrift_rpc_timeout_ms, "60000");
```

**Protocol Support**:

- **TBinaryProtocol**: Primary protocol for FE-BE communication
- **TCompactProtocol**: Available for space-efficient serialization
- **TJSONProtocol**: Available for debugging and interoperability

**Service Implementation** (from `be/src/service/backend_base.cpp`):

```cpp
void BackendServiceBase::transmit_data(TTransmitDataResult& return_val, 
                                     const TTransmitDataParams& params) {
    // Legacy Thrift-based data transmission (deprecated)
}

void BackendServiceBase::exec_plan_fragment(TExecPlanFragmentResult& return_val,
                                           const TExecPlanFragmentParams& params) {
    // Fragment execution coordination
}
```

### 2.2 bRPC (Baidu RPC) with Protocol Buffers

**Configuration Parameters**:

```cpp
// bRPC server configuration  
CONF_Int32(brpc_port, "8060");
CONF_Int32(brpc_num_threads, "-1");  // -1 means same as CPU cores
CONF_Int64(brpc_max_body_size, "2147483648");
CONF_Int64(brpc_socket_max_unwritten_bytes, "1073741824");
CONF_String_enum(brpc_connection_type, "single", "single,pooled,short");
```

**Core RPC Service** (from `be/src/service/internal_service.h`):

```cpp
class PInternalServiceImplBase : public T {
public:
    void transmit_chunk(google::protobuf::RpcController* controller,
                       const PTransmitChunkParams* request,
                       PTransmitChunkResult* response,
                       google::protobuf::Closure* done) override;
                       
    void transmit_runtime_filter(google::protobuf::RpcController* controller,
                               const PTransmitRuntimeFilterParams* request,
                               PTransmitRuntimeFilterResult* response,
                               google::protobuf::Closure* done) override;
};
```

## 3. Shuffle Join Communication Flow

### 3.1 End-to-End Data Flow

The complete communication flow for a shuffle join involves these stages:

1. **Fragment Initialization** (Thrift)
   - FE sends `TExecPlanFragmentParams` to each BE via Thrift
   - BE creates `DataStreamRecvr` instances for incoming data
   - BE initializes `DataStreamSender` channels to target fragments

2. **Data Partitioning and Transmission** (bRPC)
   - Source fragments hash-partition their data
   - `DataStreamSender::send_chunk()` serializes chunks using Protocol Buffers
   - Chunks transmitted via `transmit_chunk` RPC calls using bRPC

3. **Data Reception and Buffering**
   - `DataStreamRecvr` receives chunks via `PTransmitChunkParams`
   - Chunks deserialized and queued in `SenderQueue` implementations
   - Flow control via buffer size limits and closure-based backpressure

### 3.2 Protocol Message Structure

**Primary Data Transmission Message**:

```protobuf
// Inferred from code structure
message PTransmitChunkParams {
    PUniqueId finst_id = 1;           // Fragment instance ID
    int32 node_id = 2;                // Plan node ID
    int32 sender_id = 3;              // Sender identifier  
    int32 be_number = 4;              // Backend number
    int64 sequence = 5;               // Request sequence number
    bool eos = 6;                     // End of stream marker
    repeated ChunkPB chunks = 7;      // Serialized data chunks
    bool use_pass_through = 8;        // Pass-through optimization flag
    bool is_pipeline_level_shuffle = 9; // Pipeline-level shuffle flag
    repeated int32 driver_sequences = 10; // Driver sequence numbers
}

message ChunkPB {
    string data = 1;                  // Serialized chunk data
    int64 data_size = 2;              // Uncompressed size
    CompressionTypePB compress_type = 3; // Compression algorithm
    int64 uncompressed_size = 4;      // Size after decompression
}
```

### 3.3 Sender Logic Implementation

**Channel-based Transmission** (from `be/src/runtime/data_stream_sender.cpp`):

```cpp
Status DataStreamSender::Channel::send_one_chunk(const Chunk* chunk, bool eos, bool* is_real_sent) {
    // Serialize chunk to protobuf
    if (chunk != nullptr) {
        auto pchunk = _chunk_request.add_chunks();
        RETURN_IF_ERROR(_parent->serialize_chunk(chunk, pchunk, &_is_first_chunk));
        _current_request_bytes += pchunk->data().size();
    }
    
    // Send when threshold reached or EOS
    if (_current_request_bytes > _parent->_request_bytes_threshold || eos) {
        butil::IOBuf attachment;
        _parent->construct_brpc_attachment(&_chunk_request, &attachment);
        RETURN_IF_ERROR(_do_send_chunk_rpc(&_chunk_request, attachment));
        *is_real_sent = true;
    }
}
```

**RPC Invocation**:

```cpp
Status DataStreamSender::Channel::_do_send_chunk_rpc(PTransmitChunkParams* request, 
                                                     const butil::IOBuf& attachment) {
    request->set_sequence(_request_seq);
    _chunk_closure->cntl.set_timeout_ms(_brpc_timeout_ms);
    _chunk_closure->cntl.request_attachment().append(attachment);
    _brpc_stub->transmit_chunk(&_chunk_closure->cntl, request, 
                              &_chunk_closure->result, _chunk_closure);
    _request_seq++;
    return Status::OK();
}
```

### 3.4 Receiver Logic Implementation

**Queue Management** (from `be/src/runtime/sender_queue.cpp`):

```cpp
Status DataStreamRecvr::PipelineSenderQueue::add_chunks(const PTransmitChunkParams& request,
                                                        Metrics& metrics,
                                                        google::protobuf::Closure** done) {
    // Deserialize chunks from request
    StatusOr<ChunkList> chunks_or = get_chunks_from_request<true>(request, metrics, total_chunk_bytes);
    
    // Enqueue chunks to appropriate driver queues
    for (auto& chunk_item : chunks) {
        size_t index = _is_pipeline_level_shuffle ? chunk_item.driver_sequence : 0;
        auto& chunk_queue = _chunk_queues[index];
        chunk_queue.enqueue(std::move(chunk_item));
    }
    
    // Update flow control metrics
    _recvr->_num_buffered_bytes += total_chunk_bytes;
    return Status::OK();
}
```

## 4. Serialization and Compression

### 4.1 Chunk Serialization

StarRocks uses a custom Protocol Buffer-based serialization format optimized for columnar data:

**Serialization Process** (from `be/src/runtime/data_stream_sender.cpp`):

```cpp
Status DataStreamSender::serialize_chunk(const Chunk* chunk, ChunkPB* dst, 
                                        bool* is_first_chunk, int num_receivers) {
    // Use ProtobufChunkSerde for efficient columnar serialization
    TRY_CATCH_BAD_ALLOC({
        serde::ProtobufChunkSerde serde;
        ASSIGN_OR_RETURN(auto result, serde.serialize(*chunk, dst, _encode_level));
    });
    
    // Compression if enabled
    if (_compress_type != CompressionTypePB::NO_COMPRESSION) {
        size_t max_compressed_size = _compress_codec->max_compressed_len(dst->data().size());
        _compression_scratch.resize(max_compressed_size);
        
        Slice compressed_slice(_compression_scratch.data(), max_compressed_size);
        RETURN_IF_ERROR(_compress_codec->compress(dst->data(), &compressed_slice));
        
        dst->set_uncompressed_size(dst->data().size());
        dst->set_data(compressed_slice.data, compressed_slice.size);
        dst->set_compress_type(_compress_type);
    }
    
    return Status::OK();
}
```

### 4.2 Supported Compression Algorithms

**Available Compression Types**:

- `NO_COMPRESSION`: Raw data transmission
- `LZ4`: Fast compression/decompression (default when enabled)
- Additional algorithms via `CompressionUtils::to_compression_pb()`

**Configuration**:

```cpp
// Runtime compression selection
if (state->query_options().__isset.transmission_compression_type) {
    _compress_type = CompressionUtils::to_compression_pb(
        state->query_options().transmission_compression_type);
} else if (config::compress_rowbatches) {
    _compress_type = CompressionTypePB::LZ4;
}
```

## 5. Performance Optimizations

### 5.1 Pass-Through Optimization

StarRocks implements a pass-through optimization for local data transfer within the same process:

**Pass-Through Buffer** (from `be/src/runtime/local_pass_through_buffer.cpp`):

```cpp
class PassThroughSenderChannel {
public:
    void append_chunk(const Chunk* chunk, size_t chunk_size, int32_t driver_sequence) {
        std::lock_guard<std::mutex> guard(_mutex);
        _buffer.emplace_back(chunk->clone());
        _bytes.emplace_back(chunk_size);
        _physical_bytes += chunk_size;
    }
    
    void pull_chunks(ChunkUniquePtrVector* chunks, std::vector<size_t>* bytes) {
        std::lock_guard<std::mutex> guard(_mutex);
        chunks->swap(_buffer);
        bytes->swap(_bytes);
        _physical_bytes = 0;
    }
};
```

### 5.2 Pipeline-Level Shuffle

For pipeline execution, StarRocks supports pipeline-level shuffle to improve parallelism:

```cpp
if (_parent->_is_pipeline_level_shuffle) {
    _chunk_request->add_driver_sequences(driver_sequence);
    _chunk_request->set_is_pipeline_level_shuffle(true);
}
```

### 5.3 Batching and Flow Control

**Request Batching**:

```cpp
// Batch multiple chunks before sending
if (_current_request_bytes > config::max_transmit_batched_bytes) {
    // Send accumulated chunks
    butil::IOBuf attachment;
    construct_brpc_attachment(_chunk_request, attachment);
    for (auto idx : _channel_indices) {
        RETURN_IF_ERROR(_channels[idx]->send_chunk_request(_chunk_request, attachment));
    }
    _current_request_bytes = 0;
    _chunk_request->clear_chunks();
}
```

**Backpressure via Closures**:

```cpp
// Use closures for flow control
RefCountClosure<PTransmitChunkResult>* _chunk_closure;

// Wait for previous request before sending next
Status _wait_prev_request() {
    if (_request_seq == 0) return Status::OK();
    
    auto cntl = &_chunk_closure->cntl;
    brpc::Join(cntl->call_id());  // Block until completion
    
    if (cntl->Failed()) {
        return Status::ThriftRpcError("fail to send batch");
    }
    return {_chunk_closure->result.status()};
}
```

## 6. Error Handling and Reliability

### 6.1 Connection Management

**Recoverable Stub Implementation** (from `be/src/util/internal_service_recoverable_stub.cpp`):

```cpp
class PInternalService_RecoverableStub {
    std::shared_ptr<PInternalService_Stub> _stub;
    butil::EndPoint _endpoint;
    
    void transmit_chunk(google::protobuf::RpcController* controller,
                       const PTransmitChunkParams* request,
                       PTransmitChunkResult* response,
                       google::protobuf::Closure* done) {
        auto closure = new RecoverableClosure(shared_from_this(), controller, done);
        _stub->transmit_chunk(controller, request, response, closure);
    }
};
```

### 6.2 Timeout Configuration

**Timeout Hierarchy**:

```cpp
// Query-level timeout
_brpc_timeout_ms = std::min(3600, state->query_options().query_timeout) * 1000;

// RPC-level timeouts
CONF_Int32(thrift_rpc_timeout_ms, "60000");
CONF_mInt32(txn_commit_rpc_timeout_ms, "60000");
```

### 6.3 Error Recovery

**Retry Logic** (from `be/src/util/thrift_rpc_helper.cpp`):

```cpp
template <typename T>
Status ThriftRpcHelper::rpc(const std::string& ip, const int32_t port,
                           std::function<void(ClientConnection<T>&)> callback,
                           int timeout_ms, int retry_times) {
    int i = 0;
    do {
        status = rpc_impl(callback, client, address);
        if (status.ok()) {
            return Status::OK();
        }
        
        // Retry with backoff
        SleepFor(MonoDelta::FromMilliseconds(config::thrift_client_retry_interval_ms));
        auto st = client.reopen(timeout_ms);
        if (!st.ok()) break;
    } while (i++ < retry_times);
    
    return status;
}
```

## 7. Configuration and Tuning

### 7.1 Key Configuration Parameters

**Thrift Configuration**:

```bash
# BE Configuration (be.conf)
be_port = 9060                           # Thrift service port
thrift_rpc_strict_mode = true           # Protocol strictness
thrift_rpc_max_body_size = 2147483648   # 2GB max message size
thrift_rpc_timeout_ms = 60000           # 60s RPC timeout
```

**bRPC Configuration**:

```bash
# BE Configuration (be.conf)  
brpc_port = 8060                        # bRPC service port
brpc_num_threads = -1                   # Worker threads (auto)
brpc_max_body_size = 2147483648         # 2GB max message size
brpc_socket_max_unwritten_bytes = 1073741824  # 1GB socket buffer
brpc_connection_type = single           # Connection pooling strategy
```

**Data Transmission Configuration**:

```bash
compress_rowbatches = true              # Enable compression
max_transmit_batched_bytes = 262144     # 256KB batching threshold
transmission_encode_level = 0           # Encoding optimization level
```

### 7.2 Performance Tuning Guidelines

**For High-Throughput Scenarios**:

1. Increase `brpc_max_body_size` and `brpc_socket_max_unwritten_bytes`
2. Enable compression with `compress_rowbatches = true`
3. Tune `max_transmit_batched_bytes` based on network characteristics
4. Consider `brpc_connection_type = pooled` for connection reuse

**For Low-Latency Scenarios**:

1. Reduce `max_transmit_batched_bytes` for immediate transmission
2. Disable compression to reduce CPU overhead
3. Use `brpc_connection_type = single` for dedicated connections
4. Lower timeout values for faster failure detection

## 8. Monitoring and Observability

### 8.1 Built-in Metrics

**DataStreamSender Metrics**:

```cpp
_serialize_chunk_timer = ADD_TIMER(profile, "SerializeChunkTime");
_bytes_sent_counter = ADD_COUNTER(profile, "BytesSent", TUnit::BYTES);
_wait_response_timer = ADD_TIMER(profile, "WaitResponseTime");
_send_request_timer = ADD_TIMER(profile, "SendRequestTime");
```

**DataStreamRecvr Metrics**:

```cpp
process_total_timer = ADD_TIMER(profile, "ReceiverProcessTotalTime");
wait_lock_timer = ADD_TIMER(profile, "WaitLockTime");
deserialize_chunk_timer = ADD_TIMER(profile, "DeserializeChunkTime");
peak_buffer_mem_bytes = ADD_COUNTER(profile, "PeakBufferMemoryBytes", TUnit::BYTES);
```

### 8.2 Debugging Support

**Verbose Logging**:

```cpp
VLOG_ROW << "DataStreamSender send chunk: #rows=" << chunk->num_rows()
         << " dest=" << _brpc_dest_addr.hostname << ":" << _brpc_dest_addr.port;

VLOG_RPC << "transmit chunk: fragment_instance_id=" << print_id(request->finst_id())
         << " node=" << request->node_id() << " sender=" << request->sender_id();
```

## 9. Limitations and Considerations

### 9.1 Current Limitations

1. **Protocol Compatibility**: Thrift IDL definitions not found in workspace, limiting protocol introspection
2. **Single Protocol Buffer Schema**: All chunk data uses same serialization format regardless of data types
3. **No Built-in Encryption**: Communication occurs in plaintext (encryption must be handled at network layer)
4. **Limited Compression Options**: Primarily LZ4 support, other algorithms may have limited testing

### 9.2 Scalability Considerations

1. **Memory Pressure**: Large buffer sizes (`brpc_socket_max_unwritten_bytes`) can cause memory pressure under high concurrency
2. **Connection Limits**: Single connection type may not scale optimally for all workload patterns  
3. **Serialization Overhead**: Protocol Buffer serialization adds CPU overhead for small chunks

## 10. Future Evolution

### 10.1 Potential Improvements

1. **HTTP/2 Support**: bRPC already supports HTTP/2, could be leveraged for better multiplexing
2. **Adaptive Compression**: Dynamic compression selection based on data characteristics
3. **Zero-Copy Optimization**: Further reduction of memory copying in serialization path
4. **Enhanced Monitoring**: More granular metrics for network performance analysis

### 10.2 Industry Alignment

StarRocks' dual-protocol approach aligns with industry best practices:

- **Control/Data Plane Separation**: Similar to Kubernetes (etcd + gRPC)
- **High-Performance RPC**: bRPC comparable to gRPC for performance-critical paths
- **Protocol Buffer Usage**: Industry standard for structured data serialization

## Conclusion

StarRocks implements a sophisticated and well-architected communication layer that effectively balances performance, reliability, and operational complexity. The dual-protocol design (Thrift + bRPC/Protobuf) provides clear separation of concerns while enabling high-performance distributed join execution.

Key strengths include:

- **Optimized for Columnar Data**: Custom serialization format for analytical workloads
- **Comprehensive Flow Control**: Multiple levels of backpressure and buffering
- **Production-Ready Reliability**: Robust error handling, timeouts, and retry mechanisms
- **Performance Optimizations**: Pass-through buffers, compression, batching, and pipeline-level shuffle

The architecture demonstrates deep understanding of distributed systems challenges and provides a solid foundation for high-performance analytical query processing.

---

**Report Compilation Date**: December 2024  
**StarRocks Version Analyzed**: Latest main branch  
**Primary Source Files Examined**: 50+ files across runtime, service, and utility modules
