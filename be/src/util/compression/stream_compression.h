#pragma once

#include <memory>

#include "common/status.h"
#include "gen_cpp/types.pb.h"

namespace starrocks {

class StreamCompression {
public:
    virtual ~StreamCompression() = default;

    // implement in derived class
    // input(in):               buf where decompress begin
    // input_len(in):           max length of input buf
    // input_bytes_read(out):   bytes which is consumed by decompressor
    // output(out):             buf where to save decompressed data
    // output_len(in):      max length of output buf
    // output_bytes_written(out):   decompressed data size in output buf
    // stream_end(out):         true if reach the and of stream,
    //                          or normally finished decompressing entire block
    //
    // input and output buf should be allocated and released outside
    virtual Status decompress(uint8_t* input, size_t input_len, size_t* input_bytes_read, uint8_t* output,
                              size_t output_len, size_t* output_bytes_written, bool* stream_end) = 0;

    virtual Status compress(uint8_t* input, size_t input_len, size_t* input_bytes_read, uint8_t* output,
                            size_t output_len, size_t* output_bytes_written, bool* stream_end) {
        return Status::NotSupported("compress for StreamCompression not supported");
    }

public:
    static Status create_decompressor(CompressionTypePB type, std::unique_ptr<StreamCompression>* decompressor);

    virtual std::string debug_info() { return "StreamCompression"; }

    CompressionTypePB get_type() { return _ctype; }

    size_t get_compressed_block_size() { return _compressed_block_size; }
    size_t set_compressed_block_size(size_t size) { return _compressed_block_size = size; }

protected:
    virtual Status init() { return Status::OK(); }

    StreamCompression(CompressionTypePB ctype) : _ctype(ctype) {}

    CompressionTypePB _ctype;

    size_t _compressed_block_size = 0;
};

} // namespace starrocks
