#pragma once

#include "common/status.h"
#include "gen_cpp/internal_service.pb.h"
#include "service/internal_service.h"

namespace brpc {
class Controller;
}

namespace starrocks {

class ExecEnv;

template <typename T>
class BackendInternalServiceImpl : public PInternalServiceImplBase<T> {
public:
    BackendInternalServiceImpl(ExecEnv* exec_env) : PInternalServiceImplBase<T>(exec_env) {}

    void tablet_writer_open(google::protobuf::RpcController* controller, const PTabletWriterOpenRequest* request,
                            PTabletWriterOpenResult* response, google::protobuf::Closure* done) override;

    void tablet_writer_add_batch(google::protobuf::RpcController* controller,
                                 const PTabletWriterAddBatchRequest* request, PTabletWriterAddBatchResult* response,
                                 google::protobuf::Closure* done) override;

    void tablet_writer_add_chunk(google::protobuf::RpcController* controller,
                                 const PTabletWriterAddChunkRequest* request, PTabletWriterAddBatchResult* response,
                                 google::protobuf::Closure* done) override;

    void tablet_writer_add_chunks(google::protobuf::RpcController* controller,
                                  const PTabletWriterAddChunksRequest* request, PTabletWriterAddBatchResult* response,
                                  google::protobuf::Closure* done) override;

    void tablet_writer_add_chunk_via_http(google::protobuf::RpcController* controller, const PHttpRequest* request,
                                          PTabletWriterAddBatchResult* response,
                                          google::protobuf::Closure* done) override;

    void tablet_writer_add_chunks_via_http(google::protobuf::RpcController* controller, const PHttpRequest* request,
                                           PTabletWriterAddBatchResult* response,
                                           google::protobuf::Closure* done) override;

    void tablet_writer_add_segment(google::protobuf::RpcController* controller,
                                   const PTabletWriterAddSegmentRequest* request,
                                   PTabletWriterAddSegmentResult* response, google::protobuf::Closure* done) override;

    void tablet_writer_cancel(google::protobuf::RpcController* controller, const PTabletWriterCancelRequest* request,
                              PTabletWriterCancelResult* response, google::protobuf::Closure* done) override;

    void get_load_replica_status(google::protobuf::RpcController* controller, const PLoadReplicaStatusRequest* request,
                                 PLoadReplicaStatusResult* response, google::protobuf::Closure* done) override;

    void load_diagnose(google::protobuf::RpcController* controller, const PLoadDiagnoseRequest* request,
                       PLoadDiagnoseResult* response, google::protobuf::Closure* done) override;

    void local_tablet_reader_open(google::protobuf::RpcController* controller, const PTabletReaderOpenRequest* request,
                                  PTabletReaderOpenResult* response, google::protobuf::Closure* done) override;
    void local_tablet_reader_close(google::protobuf::RpcController* controller,
                                   const PTabletReaderCloseRequest* request, PTabletReaderCloseResult* response,
                                   google::protobuf::Closure* done) override;
    void local_tablet_reader_multi_get(google::protobuf::RpcController* controller,
                                       const PTabletReaderMultiGetRequest* request,
                                       PTabletReaderMultiGetResult* response, google::protobuf::Closure* done) override;
    void local_tablet_reader_scan_open(google::protobuf::RpcController* controller,
                                       const PTabletReaderScanOpenRequest* request,
                                       PTabletReaderScanOpenResult* response, google::protobuf::Closure* done) override;
    void local_tablet_reader_scan_get_next(google::protobuf::RpcController* controller,
                                           const PTabletReaderScanGetNextRequest* request,
                                           PTabletReaderScanGetNextResult* response,
                                           google::protobuf::Closure* done) override;
};

} // namespace starrocks
