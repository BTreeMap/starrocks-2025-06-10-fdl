// Copyright 2021-present StarRocks, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "exec/pipeline/exchange/sink_buffer.h"

#include <bthread/bthread.h>

#include <chrono>
#include <mutex>
#include <string_view>
#include <vector>
#include <algorithm>
#include <numeric>

#include "exec/pipeline/schedule/utils.h"
#include "fmt/core.h"
#include "util/defer_op.h"
#include "util/time.h"
#include "util/uid_util.h"

namespace starrocks::pipeline {

SinkBuffer::SinkBuffer(FragmentContext* fragment_ctx, const std::vector<TPlanFragmentDestination>& destinations,
                       bool is_dest_merge)
        : _fragment_ctx(fragment_ctx),
          _mem_tracker(fragment_ctx->runtime_state()->instance_mem_tracker()),
          _brpc_timeout_ms(fragment_ctx->runtime_state()->query_options().query_timeout * 1000),
          _is_dest_merge(is_dest_merge),
          _rpc_http_min_size(fragment_ctx->runtime_state()->get_rpc_http_min_size()),
          _sent_audit_stats_frequency_upper_limit(
                  std::max((int64_t)64, BitUtil::RoundUpToPowerOfTwo(fragment_ctx->total_dop() * 4))) {
    for (const auto& dest : destinations) {
        const auto& instance_id = dest.fragment_instance_id;
        // instance_id.lo == -1 indicates that the destination is pseudo for bucket shuffle join.
        if (instance_id.lo == -1) {
            continue;
        }

        if (_sink_ctxs.count(instance_id.lo) == 0) {
            _sink_ctxs[instance_id.lo] = std::make_unique<SinkContext>();
            auto& ctx = sink_ctx(instance_id.lo);
            ctx.num_sinker = 0;
            ctx.request_seq = -1;
            ctx.max_continuous_acked_seqs = -1;
            ctx.num_finished_rpcs = 0;
            ctx.num_in_flight_rpcs = 0;
            ctx.dest_addrs = dest.brpc_server;
            // allocate per-destination ring buffer for batch timing
            if (ctx.batch_capacity == 0) ctx.batch_capacity = 1024;
            ctx.batch_events = std::unique_ptr<BatchTimingEvent[]>(new BatchTimingEvent[ctx.batch_capacity]{});

            PUniqueId finst_id;
            finst_id.set_hi(instance_id.hi);
            finst_id.set_lo(instance_id.lo);
            ctx.finst_id = std::move(finst_id);
        }
    }
}

SinkBuffer::~SinkBuffer() {
    // In some extreme cases, the pipeline driver has not been created yet, and the query is over
    // At this time, sink_buffer also needs to be able to be destructed correctly
    _is_finishing = true;

    DCHECK(is_finished());

    _sink_ctxs.clear();
}

void SinkBuffer::incr_sinker(RuntimeState* state) {
    _num_uncancelled_sinkers++;
    for (auto& [_, sink_ctx] : _sink_ctxs) {
        sink_ctx->num_sinker++;
    }
    _num_remaining_eos += _sink_ctxs.size();
}

Status SinkBuffer::add_request(TransmitChunkInfo& request) {
    DCHECK(_num_remaining_eos > 0);
    if (_is_finishing) {
        return Status::OK();
    }
    if (!request.attachment.empty()) {
        _bytes_enqueued += request.attachment.size();
        _request_enqueued++;
    }
    {
        // set stats every _sent_audit_stats_frequency, so FE can get approximate stats even missing eos chunks.
        // _sent_audit_stats_frequency grows exponentially to reduce the costs of collecting stats but
        // let the first (limited) chunks' stats approach truth。
        auto request_sequence = _request_sequence++;
        if (!request.params->eos() && (request_sequence & (_sent_audit_stats_frequency - 1)) == 0) {
            if (_sent_audit_stats_frequency < _sent_audit_stats_frequency_upper_limit) {
                _sent_audit_stats_frequency = _sent_audit_stats_frequency << 1;
            }
            if (auto part_stats = _fragment_ctx->runtime_state()->query_ctx()->intermediate_query_statistic()) {
                part_stats->to_pb(request.params->mutable_query_statistics());
            }
        }

        auto& instance_id = request.fragment_instance_id;
        auto& context = sink_ctx(instance_id.lo);

        RETURN_IF_ERROR(_try_to_send_rpc(instance_id, [&]() { context.buffer.push(request); }));
    }

    return Status::OK();
}

bool SinkBuffer::is_full() const {
    // std::queue' read is concurrent safe without mutex
    // Judgement may not that accurate because we do not known in advance which
    // instance the data to be sent corresponds to
    size_t max_buffer_size = config::pipeline_sink_buffer_size * _sink_ctxs.size();
    size_t buffer_size = 0;
    for (auto& [_, context] : _sink_ctxs) {
        buffer_size += context->buffer.size();
    }
    const bool is_full = buffer_size > max_buffer_size;

    int64_t last_full_timestamp = _last_full_timestamp;
    int64_t full_time = _full_time;

    if (is_full && last_full_timestamp == -1) {
        _last_full_timestamp.compare_exchange_weak(last_full_timestamp, MonotonicNanos());
    }
    if (!is_full && last_full_timestamp != -1) {
        // The following two update operations cannot guarantee atomicity as a whole without lock
        // But we can accept bias in estimatation
        _full_time.compare_exchange_weak(full_time, full_time + (MonotonicNanos() - last_full_timestamp));
        _last_full_timestamp.compare_exchange_weak(last_full_timestamp, -1);
    }

    return is_full;
}

void SinkBuffer::set_finishing() {
    _pending_timestamp = MonotonicNanos();
}

bool SinkBuffer::is_finished() const {
    if (!_is_finishing) {
        return false;
    }

    return _num_sending_rpc == 0 && _total_in_flight_rpc == 0;
}

void SinkBuffer::update_profile(RuntimeProfile* profile) {
    auto* rpc_count = ADD_COUNTER(profile, "RpcCount", TUnit::UNIT);
    auto* rpc_avg_timer = ADD_TIMER(profile, "RpcAvgTime");
    auto* network_timer = ADD_TIMER(profile, "NetworkTime");
    auto* wait_timer = ADD_TIMER(profile, "WaitTime");
    auto* overall_timer = ADD_TIMER(profile, "OverallTime");

    COUNTER_SET(rpc_count, _rpc_count);
    COUNTER_SET(rpc_avg_timer, _rpc_cumulative_time / std::max(_rpc_count.load(), static_cast<int64_t>(1)));

    COUNTER_SET(network_timer, _network_time());

    // Add detailed timing metrics for single-way time decomposition
    auto* serialization_timer = ADD_TIMER(profile, "SerializationTime");
    auto* pure_network_timer = ADD_TIMER(profile, "PureNetworkTime");
    COUNTER_SET(serialization_timer, _detailed_serialization_time());
    COUNTER_SET(pure_network_timer, _detailed_network_time());

    COUNTER_SET(overall_timer, _last_receive_time - _first_send_time);

    // WaitTime consists two parts
    // 1. buffer full time
    // 2. pending finish time
    COUNTER_SET(wait_timer, _full_time);
    COUNTER_UPDATE(wait_timer, MonotonicNanos() - _pending_timestamp);

    auto* bytes_sent_counter = ADD_COUNTER(profile, "BytesSent", TUnit::BYTES);
    auto* request_sent_counter = ADD_COUNTER(profile, "RequestSent", TUnit::UNIT);
    COUNTER_SET(bytes_sent_counter, _bytes_sent);
    COUNTER_SET(request_sent_counter, _request_sent);

    auto* bytes_unsent_counter = ADD_COUNTER(profile, "BytesUnsent", TUnit::BYTES);
    auto* request_unsent_counter = ADD_COUNTER(profile, "RequestUnsent", TUnit::UNIT);
    COUNTER_SET(bytes_unsent_counter, _bytes_enqueued - _bytes_sent);
    COUNTER_SET(request_unsent_counter, _request_enqueued - _request_sent);

    profile->add_derived_counter(
            "NetworkBandwidth", TUnit::BYTES_PER_SECOND,
            [bytes_sent_counter, network_timer] {
                return RuntimeProfile::units_per_second(bytes_sent_counter, network_timer);
            },
            "");
    profile->add_derived_counter(
            "OverallThroughput", TUnit::BYTES_PER_SECOND,
            [bytes_sent_counter, overall_timer] {
                return RuntimeProfile::units_per_second(bytes_sent_counter, overall_timer);
            },
            "");

    // Aggregate per-batch samples into percentiles and averages (best-effort)
    std::vector<int64_t> samples_ser_ns;
    std::vector<int64_t> samples_net_ns;
    samples_ser_ns.reserve(4096);
    samples_net_ns.reserve(4096);
    int64_t payload_sum = 0;
    int64_t payload_cnt = 0;
    for (auto& [_, context] : _sink_ctxs) {
        auto& ctx = *context;
        const uint32_t wi = ctx.batch_write_idx.load(std::memory_order_relaxed);
        const uint32_t cap = std::max(1u, ctx.batch_capacity);
        const uint32_t cnt = std::min(wi, cap);
        if (!ctx.batch_events) continue;
        for (uint32_t i = wi - cnt; i < wi; ++i) {
            const auto& e = ctx.batch_events[i % cap];
            if (e.ser_done_ts == 0 || e.send_ts == 0 || e.recv_ts == 0) continue;
            int64_t ser = e.ser_done_ts - e.send_ts;
            int64_t net = e.recv_ts - e.ser_done_ts - e.receiver_proc_ns;
            if (ser < 0) ser = 0;
            if (net < 0) net = 0;
            samples_ser_ns.push_back(ser);
            samples_net_ns.push_back(net);
            if (e.payload_bytes > 0) {
                payload_sum += e.payload_bytes;
                ++payload_cnt;
            }
        }
    }

    auto percentile = [](std::vector<int64_t>& v, double p) -> int64_t {
        if (v.empty()) return 0;
        size_t n = v.size();
        size_t idx = static_cast<size_t>(std::clamp(p, 0.0, 100.0) / 100.0 * (n - 1));
        std::nth_element(v.begin(), v.begin() + idx, v.end());
        return v[idx];
    };

    int64_t ser_p50 = percentile(samples_ser_ns, 50.0);
    int64_t ser_p95 = percentile(samples_ser_ns, 95.0);
    int64_t ser_p99 = percentile(samples_ser_ns, 99.0);
    int64_t net_p50 = percentile(samples_net_ns, 50.0);
    int64_t net_p95 = percentile(samples_net_ns, 95.0);
    int64_t net_p99 = percentile(samples_net_ns, 99.0);
    int64_t payload_avg = payload_cnt > 0 ? payload_sum / payload_cnt : 0;

    auto* batch_samples = ADD_COUNTER(profile, "BatchSamples", TUnit::UNIT);
    COUNTER_SET(batch_samples, static_cast<int64_t>(samples_net_ns.size()));
    auto* batch_avg_payload = ADD_COUNTER(profile, "BatchAvgPayloadBytes", TUnit::BYTES);
    COUNTER_SET(batch_avg_payload, payload_avg);
    auto* ser_p50_timer = ADD_TIMER(profile, "BatchSerializationTimeP50");
    auto* ser_p95_timer = ADD_TIMER(profile, "BatchSerializationTimeP95");
    auto* ser_p99_timer = ADD_TIMER(profile, "BatchSerializationTimeP99");
    auto* net_p50_timer = ADD_TIMER(profile, "BatchNetworkTimeP50");
    auto* net_p95_timer = ADD_TIMER(profile, "BatchNetworkTimeP95");
    auto* net_p99_timer = ADD_TIMER(profile, "BatchNetworkTimeP99");
    COUNTER_SET(ser_p50_timer, ser_p50);
    COUNTER_SET(ser_p95_timer, ser_p95);
    COUNTER_SET(ser_p99_timer, ser_p99);
    COUNTER_SET(net_p50_timer, net_p50);
    COUNTER_SET(net_p95_timer, net_p95);
    COUNTER_SET(net_p99_timer, net_p99);
}

int64_t SinkBuffer::_network_time() {
    int64_t max = 0;
    for (auto& [_, context] : _sink_ctxs) {
        auto& time_trace = context->network_time;
        double average_concurrency =
                static_cast<double>(time_trace.accumulated_concurrency) / std::max(1, time_trace.times);
        int64_t average_accumulated_time =
                static_cast<int64_t>(time_trace.accumulated_time / std::max(1.0, average_concurrency));
        if (average_accumulated_time > max) {
            max = average_accumulated_time;
        }
    }
    return max;
}

int64_t SinkBuffer::_detailed_serialization_time() {
    int64_t max = 0;
    for (auto& [_, context] : _sink_ctxs) {
        auto& detailed_trace = context->detailed_time;
        double average_concurrency =
                static_cast<double>(detailed_trace.accumulated_concurrency) / std::max(1, detailed_trace.times);
        int64_t average_accumulated_time = static_cast<int64_t>(detailed_trace.accumulated_serialization_time /
                                                                std::max(1.0, average_concurrency));
        if (average_accumulated_time > max) {
            max = average_accumulated_time;
        }
    }
    return max;
}

int64_t SinkBuffer::_detailed_network_time() {
    int64_t max = 0;
    for (auto& [_, context] : _sink_ctxs) {
        auto& detailed_trace = context->detailed_time;
        double average_concurrency =
                static_cast<double>(detailed_trace.accumulated_concurrency) / std::max(1, detailed_trace.times);
        int64_t average_accumulated_time =
                static_cast<int64_t>(detailed_trace.accumulated_network_time / std::max(1.0, average_concurrency));
        if (average_accumulated_time > max) {
            max = average_accumulated_time;
        }
    }
    return max;
}

void SinkBuffer::cancel_one_sinker(RuntimeState* const state) {
    auto notify = this->defer_notify();
    if (--_num_uncancelled_sinkers == 0) {
        _is_finishing = true;
    }
    if (state != nullptr && state->query_ctx() && state->query_ctx()->is_query_expired()) {
        // check how many cancel operations are issued, and show the state of that time.
        VLOG_OPERATOR << fmt::format(
                "fragment_instance_id {}, _num_uncancelled_sinkers {}, _is_finishing {}, _num_remaining_eos {}, "
                "_num_sending_rpc {}, chunk is full {}",
                print_id(_fragment_ctx->fragment_instance_id()), _num_uncancelled_sinkers, _is_finishing,
                _num_remaining_eos, _num_sending_rpc, is_full());
    }
}

void SinkBuffer::_update_network_time(const TUniqueId& instance_id, const int64_t send_timestamp,
                                      const int64_t receiver_post_process_time) {
    auto& context = sink_ctx(instance_id.lo);
    const int64_t get_response_timestamp = MonotonicNanos();
    _last_receive_time = get_response_timestamp;
    int32_t concurrency = context.num_in_flight_rpcs;
    int64_t time_usage = get_response_timestamp - send_timestamp - receiver_post_process_time;
    context.network_time.update(time_usage, concurrency);
    _rpc_cumulative_time += time_usage;
    _rpc_count++;
}

void SinkBuffer::_update_detailed_time(const TUniqueId& instance_id, const int64_t send_timestamp,
                                       const int64_t serialization_timestamp,
                                       const int64_t receiver_post_process_time) {
    // This method implements the "single-way time decomposition" enhancement
    // to break down NetworkTime into SerializationTime and PureNetworkTime

    // Calculate individual timing components
    int64_t serialization_time = 0;
    if (serialization_timestamp > 0 && serialization_timestamp >= send_timestamp) {
        serialization_time = serialization_timestamp - send_timestamp;
    }
    int64_t total_round_trip = MonotonicNanos() - send_timestamp - receiver_post_process_time;
    if (total_round_trip < 0) total_round_trip = 0;
    int64_t pure_network_time = total_round_trip - serialization_time;
    if (pure_network_time < 0) pure_network_time = 0;

    // Update detailed timing metrics with concurrency level
    auto* sink_context = &sink_ctx(instance_id.lo);
    int32_t concurrency = sink_context->num_in_flight_rpcs.load();
    sink_context->detailed_time.update(serialization_time, pure_network_time, concurrency);
}

void SinkBuffer::_process_send_window(const TUniqueId& instance_id, const int64_t sequence) {
    // Both sender side and receiver side can tolerate disorder of tranmission
    // if receiver side is not ExchangeMergeSortSourceOperator
    if (!_is_dest_merge) {
        return;
    }
    auto& context = sink_ctx(instance_id.lo);
    auto& seqs = context.discontinuous_acked_seqs;
    seqs.insert(sequence);
    auto& max_continuous_acked_seq = context.max_continuous_acked_seqs;
    std::unordered_set<int64_t>::iterator it;
    while ((it = seqs.find(max_continuous_acked_seq + 1)) != seqs.end()) {
        seqs.erase(it);
        ++max_continuous_acked_seq;
    }
}

Status SinkBuffer::_try_to_send_rpc(const TUniqueId& instance_id, const std::function<void()>& pre_works) {
    auto& context = sink_ctx(instance_id.lo);
    std::lock_guard guard(context.mutex);
    pre_works();

    DeferOp decrease_defer([this]() { --_num_sending_rpc; });
    ++_num_sending_rpc;

    for (;;) {
        if (_is_finishing) {
            return Status::OK();
        }

        auto& buffer = context.buffer;

        bool too_much_brpc_process = false;
        if (_is_dest_merge) {
            // discontinuous_acked_window_size means that we are not received all the ack
            // with sequence from max_continuous_acked_seqs to request_seqs
            // Limit the size of the window to avoid buffering too much out-of-order data at the receiving side
            int64_t discontinuous_acked_window_size = context.request_seq - context.max_continuous_acked_seqs;
            too_much_brpc_process = discontinuous_acked_window_size >= config::pipeline_sink_brpc_dop;
        } else {
            too_much_brpc_process = context.num_in_flight_rpcs >= config::pipeline_sink_brpc_dop;
        }
        if (buffer.empty() || too_much_brpc_process) {
            return Status::OK();
        }

        TransmitChunkInfo& request = buffer.front();
        bool need_wait = false;
        DeferOp pop_defer([&need_wait, &buffer, mem_tracker = _mem_tracker]() {
            if (need_wait) {
                return;
            }

            // The request memory is acquired by ExchangeSinkOperator,
            // so use the instance_mem_tracker passed from ExchangeSinkOperator to release memory.
            // This must be invoked before decrease_defer desctructed to avoid sink_buffer and fragment_ctx released.
            SCOPED_THREAD_LOCAL_MEM_TRACKER_SETTER(mem_tracker);
            buffer.pop();
        });

        // The order of data transmiting in IO level may not be strictly the same as
        // the order of submitting data packets
        // But we must guarantee that first packet must be received first
        if (context.num_finished_rpcs == 0 && context.num_in_flight_rpcs > 0) {
            need_wait = true;
            return Status::OK();
        }
        if (request.params->eos()) {
            DeferOp eos_defer([this, &instance_id, &need_wait]() {
                if (need_wait) {
                    return;
                }
                if (--_num_remaining_eos == 0) {
                    auto notify = this->defer_notify();
                    _is_finishing = true;
                }
                sink_ctx(instance_id.lo).num_sinker--;
            });
            // Only the last eos is sent to ExchangeSourceOperator. it must be guaranteed that
            // eos is the last packet to send to finish the input stream of the corresponding of
            // ExchangeSourceOperator and eos is sent exactly-once.
            if (context.num_sinker > 1) {
                if (request.params->chunks_size() == 0) {
                    continue;
                } else {
                    request.params->set_eos(false);
                }
            } else {
                // The order of data transmiting in IO level may not be strictly the same as
                // the order of submitting data packets
                // But we must guarantee that eos packent must be the last packet
                if (context.num_in_flight_rpcs > 0) {
                    need_wait = true;
                    return Status::OK();
                }
                // this is the last eos query, set query stats
                if (auto final_stats = _fragment_ctx->runtime_state()->query_ctx()->intermediate_query_statistic()) {
                    final_stats->to_pb(request.params->mutable_query_statistics());
                }
            }
        }

        *request.params->mutable_finst_id() = context.finst_id;
        request.params->set_sequence(++context.request_seq);

        if (!request.attachment.empty()) {
            _bytes_sent += request.attachment.size();
            _request_sent++;
        }

        auto* closure = new TimedDisposableClosure<PTransmitChunkResult, ClosureContext>(
                {instance_id, request.params->sequence(), MonotonicNanos()});
        if (_first_send_time == -1) {
            _first_send_time = MonotonicNanos();
        }

        closure->addFailedHandler([this](const ClosureContext& ctx, std::string_view rpc_error_msg) noexcept {
            auto query_ctx = _fragment_ctx->runtime_state()->query_ctx();
            auto query_ctx_guard = query_ctx->shared_from_this();
            auto notify = this->defer_notify();

            auto defer = DeferOp([this]() { --_total_in_flight_rpc; });
            _is_finishing = true;
            auto& context = sink_ctx(ctx.instance_id.lo);
            ++context.num_finished_rpcs;
            --context.num_in_flight_rpcs;

            const auto& dest_addr = context.dest_addrs;
            std::string err_msg =
                    fmt::format("transmit chunk rpc failed [dest_instance_id={}] [dest={}:{}] detail:{}",
                                print_id(ctx.instance_id), dest_addr.hostname, dest_addr.port, rpc_error_msg);

            _fragment_ctx->cancel(Status::ThriftRpcError(err_msg));
            LOG(WARNING) << err_msg;
        });
        closure->addSuccessHandler([this, closure](const ClosureContext& ctx,
                                                   const PTransmitChunkResult& result) noexcept {
            auto query_ctx = _fragment_ctx->runtime_state()->query_ctx();
            auto query_ctx_guard = query_ctx->shared_from_this();
            auto notify = this->defer_notify();

            auto defer = DeferOp([this]() { --_total_in_flight_rpc; });
            Status status(result.status());
            auto& context = sink_ctx(ctx.instance_id.lo);
            ++context.num_finished_rpcs;
            --context.num_in_flight_rpcs;

            if (!status.ok()) {
                _is_finishing = true;
                _fragment_ctx->cancel(status);

                const auto& dest_addr = context.dest_addrs;
                LOG(WARNING) << fmt::format("transmit chunk rpc failed [dest_instance_id={}] [dest={}:{}] [msg={}]",
                                            print_id(ctx.instance_id), dest_addr.hostname, dest_addr.port,
                                            status.message());
            } else {
                // Record per-batch timing sample before any further work
                BatchTimingEvent ev{};
                ev.sequence = ctx.sequence;
                ev.send_ts = ctx.send_timestamp;
                ev.ser_done_ts = closure->serialization_timestamp.load();
                if (ev.ser_done_ts == 0) ev.ser_done_ts = ev.send_ts; // fallback for non-HTTP path
                ev.recv_ts = MonotonicNanos();
                ev.receiver_proc_ns = result.receiver_post_process_time();
                ev.payload_bytes = closure->attachment_bytes + closure->params_bytes;
                _record_batch_event(context, ev);
                static_cast<void>(_try_to_send_rpc(ctx.instance_id, [&]() {
                    _update_detailed_time(ctx.instance_id, ctx.send_timestamp, closure->serialization_timestamp.load(),
                                          result.receiver_post_process_time());
                    _update_network_time(ctx.instance_id, ctx.send_timestamp, result.receiver_post_process_time());
                    _process_send_window(ctx.instance_id, ctx.sequence);
                }));
            }
        });

        ++_total_in_flight_rpc;
        ++context.num_in_flight_rpcs;

        // Attachment will be released by process_mem_tracker in closure->Run() in bthread, when receiving the response,
        // so decrease the memory usage of attachment from instance_mem_tracker immediately before sending the request.
        _mem_tracker->release(request.attachment_physical_bytes);
        GlobalEnv::GetInstance()->process_mem_tracker()->consume(request.attachment_physical_bytes);

        closure->cntl.Reset();
        closure->cntl.set_timeout_ms(_brpc_timeout_ms);
        SET_IGNORE_OVERCROWDED(closure->cntl, query);

        Status st;
        if (bthread_self()) {
            st = _send_rpc(closure, request);
        } else {
            // When the driver worker thread sends request and creates the protobuf request,
            // also use process_mem_tracker to record the memory of the protobuf request.
            SCOPED_THREAD_LOCAL_MEM_TRACKER_SETTER(nullptr);
            // must in the same scope following the above
            st = _send_rpc(closure, request);
        }
        return st;
    }
    return Status::OK();
}

Status SinkBuffer::_send_rpc(TimedDisposableClosure<PTransmitChunkResult, ClosureContext>* closure,
                             const TransmitChunkInfo& request) {
    // capture sizes for this RPC (best-effort)
    int64_t params_size_bytes = request.params->ByteSizeLong();
    closure->params_bytes = params_size_bytes;
    closure->attachment_bytes = request.attachment.size();
    auto expected_iobuf_size = request.attachment.size() + params_size_bytes + sizeof(size_t) * 2;
    if (UNLIKELY(expected_iobuf_size > _rpc_http_min_size)) {
        butil::IOBuf iobuf;
        butil::IOBufAsZeroCopyOutputStream wrapper(&iobuf);
        request.params->SerializeToZeroCopyStream(&wrapper);
        // Capture serialization completion timestamp
        closure->serialization_timestamp.store(MonotonicNanos());
        // append params to iobuf
        size_t params_size_hdr = iobuf.size();
        closure->cntl.request_attachment().append(&params_size_hdr, sizeof(params_size_hdr));
        closure->cntl.request_attachment().append(iobuf);
        // append attachment
        size_t attachment_size = request.attachment.size();
        closure->cntl.request_attachment().append(&attachment_size, sizeof(attachment_size));
        closure->cntl.request_attachment().append(request.attachment);
        VLOG_ROW << "issue a http rpc, attachment's size = " << attachment_size
                 << " , total size = " << closure->cntl.request_attachment().size();

        if (UNLIKELY(expected_iobuf_size != closure->cntl.request_attachment().size())) {
            LOG(WARNING) << "http rpc expected iobuf size " << expected_iobuf_size << " != "
                         << " real iobuf size " << closure->cntl.request_attachment().size();
        }
        closure->cntl.http_request().set_content_type("application/proto");
        // create http_stub as needed
        auto res = HttpBrpcStubCache::getInstance()->get_http_stub(request.brpc_addr);
        if (!res.ok()) {
            return res.status();
        }
        res.value()->transmit_chunk_via_http(&closure->cntl, nullptr, &closure->result, closure);
    } else {
    // Non-HTTP path serializes inside BRPC; we can't mark serialization end here
    // Use 0 to indicate "unknown/skip" for serialization component.
    closure->serialization_timestamp.store(0);
        closure->cntl.request_attachment().append(request.attachment);
        request.brpc_stub->transmit_chunk(&closure->cntl, request.params.get(), &closure->result, closure);
    }
    return Status::OK();
}

} // namespace starrocks::pipeline
