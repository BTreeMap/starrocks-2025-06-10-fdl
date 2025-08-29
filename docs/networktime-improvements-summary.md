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

## Proposed Enhancements Roadmap (Pragmatic Version)

Emphasis shifts from heavy measurement/benchmark scaffolding to clear, common-sense improvements that (a) keep the hot path lean and (b) provide a sharper decomposition of NetworkTime.

Two tracks run in parallel and stay independently toggleable:

Track A: Lean Performance Improvements
Track B: Deeper Timing Breakdown

### Track A: Lean Performance Improvements

| Item | What Changes | Why | Effort | Risk |
|------|--------------|-----|--------|------|
| Single timestamp capture | Use one `now` for all deltas | Remove redundant syscalls | Low | None |
| Unified update function | Merge detailed+legacy updates | Fewer atomics & branches | Low | Regression if logic diverges |
| Sharded counters (power-of-two) | Array of shard structs per destination | Cut atomic contention | Med | Slight aggregation cost |
| Fixed-ratio sampling (1/N) | Skip ring recording for most RPCs | Linear memory & write reduction | Low | Distribution accuracy depends on N |
| Skip tiny payload events | Early return if bytes < threshold & concurrency=1 | Avoid noise in OLTP-like traffic | Low | Must track skipped count |
| Struct-of-arrays ring buffer | Parallel arrays per field | Better cache & selective reads | Med | Code clarity |
| Cache-line alignment | `alignas(64)` shard structs | Prevent false sharing | Low | Minor memory overhead |
| Closure pooling (optional later) | Reuse closures | Reduce alloc churn | Med | Thread-safety correctness |

Minimal new flags only (avoid explosion):
`enable_detailed_network_time`, `detailed_network_sample_n`, `detailed_network_min_payload_bytes`, `detailed_network_shards`.

### Track B: Deeper Timing Breakdown

Deliver increments that immediately add diagnostic value without requiring kernel tools first.

Priority tiers:
Tier 1 (Foundational): Serialization parity (BRPC path), Compression time, Receiver ingress delay.
Tier 2 (Refinement): Assembly/copy time, Pure network refinement (separating queue delay), RPC queue delay.
Tier 3 (Advanced Optional): Kernel send queue delay, retransmissions, RTT sampling.

| Metric | Tier | Capture Point | Notes |
|--------|------|---------------|-------|
| SerializationTime | 1 | After protobuf encode (HTTP & BRPC hook) | Parity with HTTP path |
| CompressionTime | 1 | Wrap compression codec call | Only if compression enabled |
| ReceiverIngressDelay | 1 | Receiver: socket read to start of processing | New proto optional field |
| PureNetworkTime (refined) | 2 | Use send_wire_ts if available else send_ts | Subtract serialization+receiver_proc |
| AssemblyTime | 2 | Time building attachments/iobuf | Distinguish from serialization |
| RpcQueueDelay | 2 | BRPC enqueue to send start | If BRPC exposes hook |
| KernelSendQueueDelay | 3 | eBPF tcp_sendmsg vs. ack | Advanced |
| RetransmitEvents | 3 | eBPF tracepoint | Alert metric |
| TcpSmoothedRtt | 3 | Periodic socket read / eBPF | Sampled |

### Data Structure (Succinct Sketch)

```cpp
struct alignas(64) ShardTrace {
      std::atomic<int64_t> ser_ns{0};
      std::atomic<int64_t> comp_ns{0};
      std::atomic<int64_t> assembly_ns{0};
      std::atomic<int64_t> pure_net_ns{0};
      std::atomic<int64_t> ingress_ns{0};
      std::atomic<int64_t> receiver_proc_ns{0};
      std::atomic<int64_t> samples{0};
      std::atomic<int64_t> max_pure_net_ns{0};
      std::atomic<int64_t> max_ser_ns{0};
};
```

Shard selection: `shard = seq & (num_shards - 1)`. Aggregation only when profile printed.

### Output Format (Profile Snippet Draft)

```text
NetworkTimeBreakdown (sampled 1/N=Nval, skipped_small=K):
   SerializationTime:  X ms (avg)  max=Y ms
   CompressionTime:    X ms (avg)
   AssemblyTime:       X ms (avg)
   PureNetworkTime:    X ms (avg)  max=Y ms
   ReceiverIngress:    X ms (avg)
   ReceiverProc:       X ms (avg)
```

### Incremental Execution Plan (Concrete Patches)

1. Core fast-path: timestamp reuse + unified update + sampling guard + skipped counter.
2. Sharded counters & aggregation util (no new metrics yet).
3. BRPC serialization timestamp parity + baseline SerializationTime exposure.
4. CompressionTime & AssemblyTime separation (if compression enabled code path present).
5. ReceiverIngressDelay proto addition (optional field) + profile line.
6. Advanced (only if needed): RpcQueueDelay; else stop.
7. Optional kernel/eBPF metrics behind single master flag.

Each step small & self-contained; can be reverted independently.

### Risks & Simple Mitigations

| Risk | Mitigation |
|------|-----------|
| Sampling hides tail | Track max_* counters unsampled |
| Shard count misconfigured | Clamp to power-of-two within safe bounds |
| Proto bloat | Keep new fields optional & flag-gated |
| Added branches degrade codegen | Use LIKELY/UNLIKELY and inline functions |

### Quick Wins List (Immediate)

1. Unify timing updates.
2. Add sample-N skip.
3. Add small-payload skip + counter.
4. Add shard skeleton (even if shard=1 initially) for future scaling.

After these, reassess need for deeper tracking before adding complexity.

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
