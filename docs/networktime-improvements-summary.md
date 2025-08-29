# StarRocks NetworkTime Metrics: Current Infrastructure & Improvements

## Overview

This report summarizes the CURRENT (StarRocks 3.5.x) implementation status of the NetworkTime / detailed network timing infrastructure compared to the earlier design intentions captured in `network-time-analysis.md`. It highlights what is implemented, what has been enhanced beyond the original doc, and what remains as future opportunities.

## Metric Definitions (As Implemented)

- NetworkTime (legacy aggregate): Approximates round‑trip transmission latency excluding receiver post‑processing. Computed per destination with concurrency normalization: average_network_time = accumulated_network_time / average_concurrency, final reported value is the max among destinations.
- Detailed Decomposition (NEW): Adds separation between sender‑side serialization time and pure network transit time.
- Per-Batch Telemetry (NEW, low overhead): Each successful RPC records a `BatchTimingEvent {sequence, send_ts, ser_done_ts, recv_ts, receiver_proc_ns, payload_bytes}` into a per-destination ring buffer.

## Key Code Artifacts

| Component | File | Purpose |
|-----------|------|---------|
| Core buffer & timing | `be/src/exec/pipeline/exchange/sink_buffer.{h,cpp}` | Implements RPC scheduling, ring buffer telemetry, TimeTrace & DetailedTimeTrace accumulation, decomposition logic |
| Receiver timing | `be/src/service/internal_service.cpp` | WrapClosure measures receiver processing window and returns `receiver_post_process_time` |
| Proto surface | `gensrc/proto/internal_service.proto` | Field `receiver_post_process_time` in `PTransmitChunkResult` |

## Implemented Enhancements Since Original Doc

| Area | Original Doc Status | Current Implementation | Notes |
|------|---------------------|------------------------|-------|
| Sender timestamp before serialization | Stated | Done | Closure captures `send_timestamp` immediately (`MonotonicNanos()`). |
| Exclusion of receiver processing | Stated | Done | Receiver uses WrapClosure to measure internal processing and returns duration. |
| Detailed single-way decomposition | Proposed (future) | PARTIALLY IMPLEMENTED | `_update_detailed_time()` splits serialization vs. pure network by capturing `serialization_timestamp`; does not yet expose kernel-level phases. |
| Serialization completion capture | Proposed | Done | HTTP path stores timestamp after protobuf + attachment assembly. Non-HTTP path sets 0 (unknown). |
| Concurrency-aware normalization | Stated | Done | Both `TimeTrace` and `DetailedTimeTrace` accumulate concurrency counts. |
| Per-batch sampling buffer | Not in doc | NEW | Lock-free ring buffer per destination (`batch_events`). Currently in-memory only. |
| Payload sizing | Not explicit | Done | Captures params + attachment bytes for each RPC. |
| Total RPC cumulative timing | Partial | Done | `_rpc_cumulative_time` + `_rpc_count` maintained. |
| Error isolation & backpressure | Implicit | Present | Window control via `pipeline_sink_brpc_dop` & discontinuous ack window for merge cases. |
| HTTP fast-path with manual framing | Not discussed | Present | Enables unified attachment with explicit size headers and serialization timestamp capture. |

## Current Timing Fields & Derivations

For successful RPCs:

- send_ts = closure context creation.
- ser_done_ts = timestamp after explicit protobuf + attachment serialization (HTTP path only).
- recv_ts = timestamp when success handler executes (response arrival on sender).
- receiver_proc_ns = receiver-reported processing duration.

Derived inside `_update_detailed_time`:

- serialization_time = ser_done_ts - send_ts (if ser_done_ts > 0).
- total_round_trip = now - send_ts - receiver_proc_ns.
- pure_network_time = total_round_trip - serialization_time (floored at 0).

Accumulation:

- DetailedTimeTrace holds (Σ serialization_time, Σ pure_network_time, Σ concurrency, count).
- Reported detailed network time uses same normalization heuristic as legacy metric (max over destinations of avg_network_time_component).

## Gaps vs. Earlier Enhancement Wishlist

| Proposed Future Metric | Status | Commentary |
|------------------------|--------|------------|
| Kernel send timestamp (post syscall) | NOT IMPLEMENTED | Would need instrumentation inside BRPC / socket layer or eBPF. |
| Kernel receive timestamp (pre user space) | NOT IMPLEMENTED | Same as above; could leverage SO_TIMESTAMPING + SCM_TIMESTAMPING. |
| Distinguish BRPC queuing vs. wire time | NOT IMPLEMENTED | Currently aggregated into pure_network_time. |
| eBPF TCP queue / retransmit metrics | NOT IMPLEMENTED | External perf tooling only. |
| Exposure / surfacing of per-batch ring buffer stats in profiles | PARTIAL | Buffer collected; integration into query profile not shown in current code excerpt. |
| Non-HTTP path serialization timestamp | NOT IMPLEMENTED | Would require hook before BRPC internal serialization; now recorded as 0. |
| Export of payload_bytes with timing | NOT EXPOSED | Collected per batch but not summarized yet. |
| Persist / sample ring buffer to diagnostics | NOT IMPLEMENTED | In-memory only; risk of data loss on failure. |

## Strengths of Current Infra

- Low Overhead: Minimal atomic ops; ring buffer uses relaxed ordering for best-effort analytics.
- Backward Compatible: Legacy NetworkTime behavior retained; enhanced metrics additive.
- Concurrency Sensitivity: Adjusts for parallel in-flight RPCs to avoid overstating latency.
- Serialization Attribution: Differentiates CPU serialization overhead (on HTTP path) from network transit.
- Receiver Isolation: Robust exclusion of receiver processing without cross-node clock sync.

## Limitations & Edge Cases

- Asymmetry Across Paths: HTTP fast-path benefits from detailed timing; pure BRPC path lacks serialization timestamp (0 => collapses into network time).
- Approximation of Receive Timestamp: Uses handler execution time, which includes BRPC dispatch overhead (not isolated separately).
- No Retry Differentiation: Retries (if any upstream) would aggregate into longer perceived network time without tagging.
- Potential Underflow Guards: Floors negative derived times to 0; may hide subtle ordering anomalies.
- No Direct Exposure of Distribution: Aggregation (Σ / concurrency) loses per‑RPC variance unless ring buffer exported.

## Recommended Next Steps

1. Unified Serialization Timestamp
   - Add hook for non-HTTP BRPC path (custom controller extension or patch BRPC) to record serialization finish.
2. Expose Batch Samples
   - Optional profile section: summarize P50/P95 serialization, network, payload size.
3. Kernel-Level Phase Separation
   - Introduce optional eBPF module collecting TCP send queue delay, RTT variance, retransmits; correlate via 4‑tuple + seq hash.
4. BRPC Internal Queuing Metrics
   - Instrument connection acquisition wait, controller scheduling delay.
5. Adaptive Sampling
   - Down-sample batch events when RPC rate high (e.g., reservoir sampling) to limit memory.
6. Payload-Aware Optimization Hints
   - Auto-suggest enabling compression when high pure_network_time with large payloads; disable when serialization dominates.
7. Export Structured Telemetry
   - Emit structured JSON (debug endpoint) for recent N batch events per fragment instance for live debugging.
8. Distinguish Retry / Error Paths
   - Tag events with retry count; exclude failed attempts from latency medians but keep separate counters.

## Quick Reference: Data Flow

1. Exchange sink enqueues chunk requests into `SinkBuffer`.
2. `_try_to_send_rpc` schedules RPC respecting flow control & concurrency limits.
3. Closure created (captures `send_timestamp`).
4. (HTTP path) Manual serialization => sets `serialization_timestamp`.
5. Receiver executes `_transmit_chunk`; WrapClosure measures receiver processing time.
6. Sender success handler:
   - Records `BatchTimingEvent`.
   - Updates detailed metrics (serialization & pure network) and legacy network time.
   - `_process_send_window` updates ack tracking.
7. Profile update (not shown here) reads aggregated counters.

## Summary

The NetworkTime infrastructure has evolved from a single aggregate latency metric into a richer, decomposable telemetry system featuring:

- Concurrency-normalized aggregate latency.
- Partial single-way decomposition (serialization vs. network) on the HTTP path.
- Per-batch event sampling infrastructure ready for future exposure.

Remaining work centers on deeper phase isolation (kernel & BRPC internals), path parity (non-HTTP serialization timing), and surfacing the collected fine-grained data for diagnostics and automated optimization.

---

Maintainer Guidance: Treat current detailed timing as beta; avoid over-optimizing decisions on pure_network_time until serialization timestamp parity and variance reporting are implemented.

## Proposed Enhancements Roadmap (Draft)

This section outlines concrete improvement ideas along the two requested axes:

1. Lower overhead / higher performance of instrumentation (keep hot path lean).
2. Richer, more granular breakdown of network timing (actionable diagnostics).

Each proposal lists: Objective, Approach, Est. Overhead Impact, Complexity, and Risks. Phases are ordered to deliver value early while containing risk.

### Phase 0: Baseline & Guardrails (Pre‑Change)

| Item | Objective | Approach | Deliverable |
|------|-----------|----------|-------------|
| Microbenchmark harness | Quantify current per-RPC overhead | Add gtest / benchmark that simulates N RPC completions with/without instrumentation | `be/test/pipeline/network_time_benchmark.cpp` |
| Config flags inventory | Centralize tunables | Introduce `config::enable_detailed_network_time` & sampling ratio | Updated `be/conf/be.conf` docs |
| Overhead budget definition | Set target (<1% CPU) | Measure cycles added per RPC on representative payload sizes | Summary in docs |

### Phase 1: Instrumentation Overhead Reduction

| Proposal | Objective | Approach | Est. Gain | Complexity | Notes |
|----------|----------|----------|-----------|------------|-------|
| Adaptive sampling | Reduce constant per-RPC cost | Maintain exponential moving variance of (latency, payload); sample 100% until stable then downsample to target error bound | 30–70% fewer recorded events in steady state | Medium | Must still accumulate aggregate sums accurately via scaling |
| Per-core sharded accumulators | Lower contention on atomics | Allocate `DetailedTimeTrace` shards per CPU (or bthread key) and aggregate lazily on profile read | 10–30% less atomic traffic under high fan-out | Medium | Merge cost deferred to read path |
| Branchless fast path | Minimize branching when disabled | Wrap detailed timing in `if (LIKELY(!enabled)) return` style early exits; ensure measurement code separate TU for inlining control | ~5–10% faster when disabled | Low | Keep flag in a hot-cache global |
| Compact event representation | Shrink cache footprint | Use struct-of-arrays ring buffer for timestamps & payload_size (separate arrays) to reduce write bandwidth | 5–15% memory & write bandwidth reduction | Medium | Minor code complexity increase |
| Deferred monotonic now | Avoid extra `MonotonicNanos()` | Reuse closure send_ts for event; only call another now() once (not twice) and pass to both legacy + detailed update | 1 syscall-equivalent saved per RPC | Low | Already partially done; verify duplication removal |
| Batched atomic updates | Reduce atomic increments | Locally buffer per-RPC timing deltas in thread-local; flush every K ops or on context switch | 5–20% lower atomic overhead | Medium | Need safe flush on thread exit |
| Static payload size thresholds | Skip small messages | If payload < configurable cutoff and network_time < small_latency_threshold, skip detailed event (still add to aggregates) | Up to 40% event skip on OLTP-like small batches | Medium | Ensure bias correction via scaling factors |

### Phase 2: Richer Breakdown (Application + Transport)

| Layer | New Metric | Collection Strategy | Exposure |
|-------|------------|---------------------|----------|
| Serialization | `SerializationTime` parity for BRPC path | Hook / patch before BRPC encodes protobuf (custom Controller extension or trampoline into our code) | Operator profile & debug endpoint |
| Compression | `CompressionTime` & `CompressedBytes` | Wrap codec call (if compression enabled) measure before/after | Profile child counters |
| Copy / Buffer build | `PayloadAssemblyTime` | Time constructing IOBuf / attachments (already partially in serialization) but separate compression from pure serialization | Debug JSON only initially |
| BRPC queue | `RpcQueueDelay` | Timestamp at enqueue + timestamp at network send callback (if accessible) | Optional, flag-guarded |
| Kernel send queue | `KernelSendQueueDelay` | eBPF kprobe on `tcp_sendmsg` entry/exit + track time until packets acked or timestamped | Advanced diagnostics endpoint |
| RTT estimate | `TcpSmoothedRtt` snapshot | Read from `/proc/net/tcp` or eBPF `tcp_sock` helper per connection occasionally (sampled) | Low-frequency gauge |
| Retransmissions | `RetransmitEvents` | eBPF tracepoint `tcp_retransmit_skb` filtered by 4‑tuple | Alerting metric |
| Receiver early queue | `ReceiverIngressDelay` | Additional field: receiver records time from socket read to start of processing (requires extra timestamp capture before heavy work) | Added to response proto next version |
| Clock skew safety | `NegativeDerivedEvents` | Count events where computed pure_network_time < 0 before clamping | Internal counter |

### Phase 3: Analytical & User-Facing Enhancements

| Feature | Description | Benefit |
|---------|-------------|---------|
| Percentile snapshotting | Reservoir or CKMS sketch over pure network & serialization times | Surface P50/P95 in profiles w/out storing all samples |
| Anomaly tagging | Flag events > (median + N*IQR) | Immediate visibility into outliers (e.g., pauses) |
| Auto-advice engine | Heuristics: if SerializationTime/PureNetworkTime > ratio threshold -> recommend compression toggle | Guided tuning |
| System table (`information_schema.network_time_samples`) | Expose rolling window | SQL-based troubleshooting |
| Debug REST endpoint | `/api/debug/network_time?fragment=<id>` returns JSON of last N events | Integrates with external tooling |

### Data Structures & Algorithmic Adjustments

- Ring Buffer Upgrade: Convert from single ring per destination to (destination x shard) ring to reduce false sharing; expose consumer API for snapshot.
- Accurate Scaling for Sampling: Maintain unsampled aggregate counters (sums, counts) separately from sampled distribution to avoid skew.
- Variance-Guided Sampling: Adjust sampling probability p such that relative standard error (RSE) target (e.g., <5%) is met: `p = min(1, (sigma^2 / (epsilon^2 * mu^2 * n_total)))`.
- Cache-Line Alignment: Align frequently written structs (`DetailedTimeTrace` shards) to 64 bytes; pack rarely-updated fields elsewhere.
- Monotonic Clock Batch: Provide inline `FastNow()` that caches `MonotonicNanos()` result per bthread iteration if multiple metrics need same tick.

### API / Proto Evolution Plan

| Version | Change | Compatibility Strategy |
|---------|--------|------------------------|
| v1 (current) | `receiver_post_process_time` | Baseline |
| v2 | Add optional `receiver_ingress_delay` | Backward compatible (optional field) |
| v2 | Add `serialization_complete_ts` on receiver (optional) | Only filled if enable flag set |
| v3 (optional) | Structured multi-phase timing message `PNetworkTimingDetail` with repeated key/value pairs | Sender falls back to legacy if field absent |

### Configuration Additions (Draft)

| Config Key | Type | Default | Purpose |
|------------|------|---------|---------|
| `enable_detailed_network_time` | bool | true | Master switch for current decomposition |
| `network_time_sample_ratio` | double | 1.0 | Global sampling probability for batch events |
| `network_time_adaptive_sampling` | bool | true | Enable dynamic sampling control |
| `network_time_target_rse_pct` | double | 5.0 | Target relative std error for latency estimates |
| `network_time_min_payload_bytes_for_event` | int | 0 | Skip very small payloads below threshold |
| `network_time_enable_ebpf` | bool | false | Gate kernel/eBPF instrumentation |
| `network_time_ring_shards` | int | 4 | Number of per-destination shard buffers |

### Expected Overhead Model (Illustrative)

| Component | Current (ns per RPC) | Optimized Target | Notes |
|-----------|----------------------|------------------|-------|
| 2× `MonotonicNanos()` | ~140 | 70 | Reuse timestamp / batch calls |
| Atomic increments (3–4) | 80–120 | 40–60 | Sharded + batched flush |
| Ring buffer write | 90 | 50 | SOA + sampling |
| Total incremental | 310–350 | 160–200 | Goal < 1% typical RPC budget |

### Risk & Mitigation

| Risk | Impact | Mitigation |
|------|--------|-----------|
| Sampling biases aggregate | Misleading latency averages | Maintain unsampled sum/count; only sample for distribution |
| eBPF destabilizes nodes | Kernel crashes / perf hit | Off by default; runtime safety checks; version guard |
| Added proto fields increase wire size | Slight overhead | Fields optional; only set when enabled |
| Sharding complicates lifecycle | Memory leaks or stale shards | RAII wrapper; aggregate on destruction |
| Over-instrumentation in hot path | Latency regression | Feature flags + CI microbench gating |

### Incremental Delivery Plan

1. (P0) Benchmark harness + config flags + timestamp reuse.
2. (P1a) Sharded accumulators + adaptive sampling (static heuristics first).
3. (P1b) Percentile sketch (CKMS) integrated into profile printing.
4. (P2a) BRPC path serialization hook; achieve parity across transport paths.
5. (P2b) Optional receiver ingress delay proto field.
6. (P2c) Payload/compression timing separation.
7. (P3) eBPF optional module + debug endpoint + system table.

### Acceptance Criteria (Per Phase)

- Phase 1: Demonstrate <0.5% CPU overhead at 50k RPC/s with sampling enabled (benchmark log + doc update).
- Phase 2: Provide profile snippet showing separate Serialization vs Network vs ReceiverIngress (if enabled) counters.
- Phase 3: Provide JSON endpoint returning top latency outliers with breakdown fields; eBPF path shows retransmit count >0 when induced.

### Open Design Questions

1. Should percentile sketch be per destination or aggregated? (Lean: aggregated + optional per hot dest on demand.)
2. Do we need persistence of samples across fragment lifetime? (Probably best-effort only.)
3. How to expose adaptive sampling state (current p) to users? (Add to profile footer.)
4. Governance of proto evolution—batch or incremental? (Batch in minor version to reduce churn.)

### Quick Win Recommendations (Minimal Code Changes First)

1. Reuse `MonotonicNanos()` result across `_update_detailed_time` and `_update_network_time`.
2. Add master flag to allow completely disabling detailed path in production incidents.
3. Introduce sampling ratio (even if fixed) to reduce event ring pressure.
4. Add negative time anomaly counter to validate correctness before deeper changes.

---

If you approve this roadmap, next step will be to implement Phase 0 + initial Phase 1 items (flags, timestamp reuse, sampling ratio, sharded accumulators) in small, reviewable patches.
