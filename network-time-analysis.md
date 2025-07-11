# NetworkTime Metrics Analysis in StarRocks

## Executive Summary

The NetworkTime metric in StarRocks measures the round-trip time for data exchange between compute nodes during query execution, specifically capturing the time from when a data transmission RPC is initiated until the response is received, minus the receiver's post-processing time. **Serialization time is included** in this measurement, but **deserialization time on the receiver side is excluded**.

## Detailed Analysis

### What NetworkTime Measures

NetworkTime represents the network latency for inter-node data exchange operations during distributed query execution. The calculation is:

```cpp
NetworkTime = response_received_timestamp - send_timestamp - receiver_post_process_time
```

Where:

- `send_timestamp`: Captured when the RPC closure is created (before serialization)
- `response_received_timestamp`: Captured when the RPC response is received
- `receiver_post_process_time`: Time spent on the receiver side processing the request (measured by the receiver and sent back in the RPC response)

### Key Components Included in NetworkTime

1. **Serialization Time**: ✅ **INCLUDED**
   - Protobuf serialization of `PTransmitChunkParams`
   - Data compression (if enabled)
   - Buffer preparation and attachment

2. **Network Transmission Time**: ✅ **INCLUDED**
   - Actual network latency between nodes
   - TCP/HTTP transport overhead
   - BRPC framework overhead

3. **Receiver-side Processing**: ❌ **EXCLUDED**
   - Data deserialization
   - Chunk processing and queuing
   - This time is subtracted via `receiver_post_process_time`

### Source Code Implementation

#### Sender Side (SinkBuffer)

**File**: `/config/repositories/starrocks/be/src/exec/pipeline/exchange/sink_buffer.cpp`

The timing measurement begins when the RPC closure is created:

```cpp
// File: be/src/exec/pipeline/exchange/sink_buffer.cpp, Line 349-350
// Timestamp captured BEFORE serialization
auto* closure = new DisposableClosure<PTransmitChunkResult, ClosureContext>(
        {instance_id, request.params->sequence(), MonotonicNanos()});
```

Serialization occurs afterward in `_send_rpc()`:

```cpp
// File: be/src/exec/pipeline/exchange/sink_buffer.cpp, Line 434
// Serialization happens AFTER timestamp capture
request.params->SerializeToZeroCopyStream(&wrapper);
```

The NetworkTime is calculated when the response is received:

```cpp
// File: be/src/exec/pipeline/exchange/sink_buffer.cpp, Line 233
// NetworkTime calculation in _update_network_time()
int64_t time_usage = get_response_timestamp - send_timestamp - receiver_post_process_time;
context.network_time.update(time_usage, concurrency);
```

#### Receiver Side (Internal Service)

**File**: `/config/repositories/starrocks/be/src/service/internal_service.cpp`

The receiver measures its own processing time:

```cpp
// File: be/src/service/internal_service.cpp, Lines 97-106
// Receiver timing in WrapClosure class
class WrapClosure : public google::protobuf::Closure {
    // ...
    void Run() override {
        const auto response_timestamp = MonotonicNanos();
        _response->set_receiver_post_process_time(response_timestamp - _receive_timestamp);
    }
private:
    const int64_t _receive_timestamp = MonotonicNanos(); // Set on construction
};
```

### Receiver-side Processing Time Synchronization

**Critical Understanding**: The `receiver_post_process_time` is **measured on the receiver side** and **sent back to the sender** via the RPC response. This enables accurate NetworkTime calculation despite the distributed nature of the measurement.

#### How Synchronization Works

1. **Receiver Measures Its Own Processing Time**:

   ```cpp
   // File: be/src/service/internal_service.cpp, Lines 114 & 105
   // When RPC request arrives, capture receive timestamp
   const int64_t _receive_timestamp = MonotonicNanos(); // Set on closure construction
   
   // When processing is complete, capture response timestamp  
   const auto response_timestamp = MonotonicNanos();
   
   // Calculate and embed processing time in the response protobuf
   _response->set_receiver_post_process_time(response_timestamp - _receive_timestamp);
   ```

2. **Sender Extracts Processing Time from Response**:

   ```cpp
   // File: be/src/exec/pipeline/exchange/sink_buffer.cpp, Line 395
   closure->addSuccessHandler([this](const ClosureContext& ctx, const PTransmitChunkResult& result) {
       // Extract receiver's processing time from the RPC response
       _update_network_time(ctx.instance_id, ctx.send_timestamp, result.receiver_post_process_time());
   });
   ```

3. **Pure Network Time Calculation**:

   ```cpp
   // File: be/src/exec/pipeline/exchange/sink_buffer.cpp, Line 233
   // Subtract receiver processing time to get pure network + serialization time
   int64_t time_usage = get_response_timestamp - send_timestamp - receiver_post_process_time;
   ```

This design ensures that:

- **Receiver-side deserialization and processing** is excluded from NetworkTime
- **Network latency and sender-side serialization** is accurately captured
- **No separate communication channel** is needed (timing data piggybacks on the RPC response)
- **Clock synchronization between nodes** is not required (each node uses its own monotonic clock)

### Concurrency Handling

NetworkTime accounts for concurrent RPCs to provide accurate measurements:

```cpp
// File: be/src/exec/pipeline/exchange/sink_buffer.cpp, Lines 201-204
// Average concurrency calculation to adjust for parallel transmissions
double average_concurrency =
        static_cast<double>(time_trace.accumulated_concurrency) / std::max(1, time_trace.times);
int64_t average_accumulated_time =
        static_cast<int64_t>(time_trace.accumulated_time / std::max(1.0, average_concurrency));
```

The final NetworkTime reported is the **maximum** average accumulated time among all destinations (Line 206-208 in the same function).

### Usage Context

NetworkTime is primarily measured in:

1. **Exchange Operators**: During inter-fragment data transmission
2. **Data Shuffling**: When redistributing data across compute nodes  
3. **Distributed Joins**: For exchanging join data between nodes

The metric appears in query profiles as:

- Individual operator `NetworkTime`
- Aggregated `QueryCumulativeNetworkTime` across all Exchange nodes

### Historical Context

According to release notes (v2.3.14, v3.0.2), StarRocks **removed the dependency on system clocks** to fix incorrect NetworkTime measurements caused by clock skew between servers. The current implementation uses `MonotonicNanos()` which provides monotonic, consistent timing.

### Performance Implications

NetworkTime includes serialization overhead, which means:

- **High NetworkTime** may indicate:
  - Network congestion or high latency
  - Large data transfers requiring significant serialization
  - Inefficient data exchange patterns
  
- **Optimization considerations**:
  - Data compression can reduce network transfer time but increase serialization time
  - The trade-off is captured in the NetworkTime metric
  - Monitoring NetworkTime helps identify network vs. compute bottlenecks

### Related Files

| File | Purpose |
|------|---------|
| `be/src/exec/pipeline/exchange/sink_buffer.cpp` | Main NetworkTime measurement implementation |
| `be/src/exec/pipeline/exchange/sink_buffer.h` | TimeTrace structure and NetworkTime documentation |
| `be/src/service/internal_service.cpp` | Receiver-side processing time measurement |
| `be/src/runtime/data_stream_mgr.cpp` | Data stream management and chunk processing |
| `be/src/exec/pipeline/exchange/exchange_sink_operator.cpp` | Exchange sink operator using SinkBuffer |

### Conclusion

NetworkTime provides a comprehensive view of data exchange performance in StarRocks, including both network latency and serialization overhead while excluding receiver-side processing. This design allows for accurate bottleneck identification in distributed query execution, helping distinguish between network-related performance issues and local computation overhead.

### Advanced Network Time Breakdown and Measurement Techniques

While the current NetworkTime metric provides valuable insight into distributed query performance, it aggregates several components into a single measurement. This section explores techniques to break down NetworkTime into more granular metrics for deeper performance analysis.

#### Current NetworkTime Composition Analysis

Based on the implementation, NetworkTime currently includes:

1. **Sender-side Serialization** (✅ Included)
   - Protobuf message serialization (`request.params->SerializeToZeroCopyStream()`)
   - Data compression (if enabled)
   - BRPC request preparation and attachment handling

2. **Network Stack Processing** (✅ Included)
   - TCP socket send/receive buffer operations
   - Kernel network stack processing (IP routing, TCP congestion control)
   - Network device driver queuing and transmission
   - Physical network transmission time

3. **BRPC Framework Overhead** (✅ Included)
   - Connection management and multiplexing
   - Request/response correlation and routing
   - BRPC internal queuing and scheduling

4. **Receiver-side Processing** (❌ Excluded)
   - Message deserialization
   - Chunk processing and queuing
   - Application-level response preparation

#### Proposed Enhanced Measurement Framework

##### 1. Single-Way Time Decomposition

To separate round-trip time into directional components, additional timestamps could be added:

```cpp
// Enhanced timing points in sink_buffer.cpp
struct DetailedNetworkTiming {
    int64_t send_timestamp;           // Current: RPC closure creation
    int64_t serialization_complete;   // NEW: After SerializeToZeroCopyStream()
    int64_t kernel_send_timestamp;    // NEW: After successful send() syscall
    int64_t kernel_recv_timestamp;    // NEW: When response arrives at kernel
    int64_t response_timestamp;       // Current: Response received
    int64_t receiver_process_time;    // Current: From receiver
};

// Example implementation locations:
// File: be/src/exec/pipeline/exchange/sink_buffer.cpp
void SinkBuffer::_send_rpc_with_detailed_timing(...) {
    timing.send_timestamp = MonotonicNanos();
    
    // Serialize request
    request.params->SerializeToZeroCopyStream(&wrapper);
    timing.serialization_complete = MonotonicNanos();
    
    // BRPC call with custom completion callback
    stub->transmit_chunk(&closure->cntl, request.params, &closure->response, closure);
    // Could hook into BRPC's OnSendComplete callback for kernel_send_timestamp
}
```

**Derived Metrics:**

- `SerializationTime = serialization_complete - send_timestamp`
- `KernelSendTime = kernel_send_timestamp - serialization_complete`
- `NetworkRoundTripTime = kernel_recv_timestamp - kernel_send_timestamp`
- `BRPCOverhead = response_timestamp - kernel_recv_timestamp`

##### 2. eBPF-based Network Stack Instrumentation

eBPF programs can provide kernel-level visibility into network performance:

```c
// Example eBPF program for TCP timing analysis
// File: tools/network_analysis/tcp_timing.bpf.c
struct tcp_timing_event {
    __u64 timestamp;
    __u32 pid;
    __u32 src_ip, dst_ip;
    __u16 src_port, dst_port;
    __u32 seq_num;
    __u8 event_type; // SEND_ENTRY, SEND_EXIT, RECV_ENTRY, RECV_EXIT
};

// Hook into TCP send path
SEC("kprobe/tcp_sendmsg")
int trace_tcp_send_entry(struct pt_regs *ctx) {
    // Capture when StarRocks data enters TCP send path
    struct tcp_timing_event event = {
        .timestamp = bpf_ktime_get_ns(),
        .event_type = SEND_ENTRY,
        // ... extract connection details
    };
    bpf_perf_event_output(ctx, &tcp_events, BPF_F_CURRENT_CPU, &event, sizeof(event));
    return 0;
}

// Hook into TCP receive path  
SEC("kprobe/tcp_recvmsg")
int trace_tcp_recv_entry(struct pt_regs *ctx) {
    // Capture when response data arrives at TCP layer
    // ... similar implementation
}
```

**eBPF Insights Available:**

- **TCP Queue Times**: Time spent in send/receive buffers
- **Congestion Control Events**: Slow start, fast retransmit, ECN marking
- **Packet Retransmissions**: Network reliability issues
- **Interrupt Processing**: Time from NIC interrupt to user space
- **CPU Scheduling Delays**: Impact of system load on network processing

##### 3. BRPC Framework Instrumentation

Enhanced timing within BRPC components:

```cpp
// Enhanced BRPC timing hooks
// File: Custom BRPC instrumentation layer
class StarRocksNetworkProfiler {
public:
    struct BRPCTiming {
        int64_t connection_acquire_time;
        int64_t request_queue_time;
        int64_t send_buffer_time;
        int64_t response_parse_time;
    };
    
    // Hook into BRPC Controller lifecycle
    void OnRequestStart(brpc::Controller* cntl) {
        auto* timing = new BRPCTiming();
        timing->connection_acquire_time = MonotonicNanos();
        cntl->set_private_data(timing);
    }
    
    void OnSendComplete(brpc::Controller* cntl) {
        auto* timing = static_cast<BRPCTiming*>(cntl->private_data());
        timing->send_buffer_time = MonotonicNanos();
    }
};
```

##### 4. Kernel Network Stack Deep Dive

Integration with kernel tracing for comprehensive network analysis:

```bash
# Example: Using ftrace for network stack analysis
echo 'function_graph' > /sys/kernel/debug/tracing/current_tracer
echo '__netif_receive_skb_core' > /sys/kernel/debug/tracing/set_graph_function
echo 'tcp_sendmsg' >> /sys/kernel/debug/tracing/set_graph_function

# Monitor StarRocks network activity
cat /sys/kernel/debug/tracing/trace_pipe | grep -E "(starrocks|PID_OF_BE_PROCESS)"
```

**Kernel-level Metrics:**

- **Network Device Queue Delays**: Time in device driver TX/RX queues
- **Interrupt Coalescing Effects**: Impact of NIC interrupt batching
- **NUMA Effects**: Cross-socket memory access penalties
- **CPU Cache Effects**: Impact of network buffer cache misses

#### Implementation Strategy

##### Phase 1: Enhanced Application-Level Timing

```cpp
// Modify sink_buffer.cpp to add serialization timing
void SinkBuffer::_send_rpc(DisposableClosure<PTransmitChunkResult, ClosureContext>* closure,
                          const TransmitChunkInfo& request) {
    auto& timing = closure->context.detailed_timing;
    timing.send_timestamp = MonotonicNanos();
    
    // ... existing serialization code ...
    timing.serialization_complete = MonotonicNanos();
    
    // Hook BRPC callbacks for kernel timing
    closure->cntl.set_timeout_ms(timeout_ms);
    closure->cntl.OnSendComplete([&timing]() {
        timing.kernel_send_timestamp = MonotonicNanos();
    });
}
```

##### Phase 2: eBPF Integration

```cpp
// Integration layer for eBPF events
class eBPFNetworkProfiler {
    void StartMonitoring(int be_pid) {
        // Load eBPF program targeting StarRocks BE process
        // Correlate eBPF events with application timestamps
    }
    
    NetworkStackTiming GetDetailedTiming(const ConnectionInfo& conn) {
        // Retrieve kernel-level timing data for specific connection
    }
};
```

##### Phase 3: Comprehensive Metrics Dashboard

**New Metrics Available:**

- `NetworkTime.Serialization`: Pure serialization overhead
- `NetworkTime.KernelSend`: Kernel send path processing
- `NetworkTime.WireTime`: Actual network transmission
- `NetworkTime.KernelRecv`: Kernel receive path processing  
- `NetworkTime.BRPCOverhead`: Framework processing overhead
- `NetworkTime.TCPRetransmissions`: Network reliability impact
- `NetworkTime.CongestionControl`: TCP flow control effects

#### Performance Analysis Benefits

This enhanced measurement framework enables:

1. **Precise Bottleneck Identification**
   - Distinguish between network congestion vs. serialization overhead
   - Identify kernel-level vs. application-level performance issues
   - Detect NUMA and CPU scheduling effects on network performance

2. **Network Infrastructure Optimization**
   - Tune TCP parameters based on actual congestion control behavior
   - Optimize NIC interrupt coalescing for StarRocks workloads
   - Identify optimal BRPC connection pool sizing

3. **Query Performance Correlation**
   - Correlate network performance with query complexity
   - Identify data size thresholds where network becomes the bottleneck
   - Optimize data partitioning strategies based on network characteristics

4. **Proactive Monitoring**
   - Detect network degradation before it impacts query performance  
   - Monitor network stack health across the cluster
   - Alert on abnormal retransmission or congestion patterns

#### Implementation Considerations

**Performance Impact**: eBPF and detailed timing add measurement overhead (~1-5% CPU increase)

**Complexity**: Requires kernel-level programming expertise and careful correlation between user-space and kernel events

**Portability**: eBPF programs may need adaptation for different kernel versions and network drivers

**Storage**: Detailed timing data significantly increases profiling data volume

This enhanced framework transforms NetworkTime from a single aggregated metric into a comprehensive network performance analysis toolkit, enabling precise optimization of StarRocks' distributed query execution performance.
