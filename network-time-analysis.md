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
- `receiver_post_process_time`: Time spent on the receiver side processing the request

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
// Line 349-350: Timestamp captured BEFORE serialization
auto* closure = new DisposableClosure<PTransmitChunkResult, ClosureContext>(
    {instance_id, request.params->sequence(), MonotonicNanos()});
```

Serialization occurs afterward in `_send_rpc()`:

```cpp
// Lines 434-435: Serialization happens AFTER timestamp capture
request.params->SerializeToZeroCopyStream(&wrapper);
```

The NetworkTime is calculated when the response is received:

```cpp
// Lines 232-234: NetworkTime calculation
int64_t time_usage = get_response_timestamp - send_timestamp - receiver_post_process_time;
context.network_time.update(time_usage, concurrency);
```

#### Receiver Side (Internal Service)

**File**: `/config/repositories/starrocks/be/src/service/internal_service.cpp`

The receiver measures its own processing time:

```cpp
// Lines 100-105: Receiver timing
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

### Concurrency Handling

NetworkTime accounts for concurrent RPCs to provide accurate measurements:

```cpp
// Average concurrency calculation to adjust for parallel transmissions
double average_concurrency = static_cast<double>(time_trace.accumulated_concurrency) / std::max(1, time_trace.times);
int64_t average_accumulated_time = static_cast<int64_t>(time_trace.accumulated_time / std::max(1.0, average_concurrency));
```

The final NetworkTime reported is the **maximum** average accumulated time among all destinations.

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
