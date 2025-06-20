#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "librdkafka/rdkafka.h"
#include "pulsar/Client.h"
#include "runtime/message_body_sink.h"
#include "runtime/stream_load/stream_load_pipe.h"
#include "simdjson.h"

namespace starrocks {

class KafkaConsumerPipe : public StreamLoadPipe {
public:
    KafkaConsumerPipe(size_t max_buffered_bytes = 1024 * 1024, size_t min_chunk_size = 64 * 1024)
            : StreamLoadPipe(max_buffered_bytes, min_chunk_size) {}

    ~KafkaConsumerPipe() override = default;

    Status append_with_row_delimiter(const char* data, size_t size, char row_delimiter) {
        return append_with_row_delimiter(data, size, row_delimiter, -1, -1);
    }

    Status append_with_row_delimiter(const char* data, size_t size, char row_delimiter, int32_t partition,
                                     int64_t offset) {
        // TODO support to add partition and offset to buffer meta
        Status st = append(data, size);
        if (!st.ok()) {
            return st;
        }

        // append the row delimiter
        st = append(&row_delimiter, 1);
        return st;
    }

    Status append_json(const char* data, size_t size, char row_delimiter) {
        return append_json(data, size, row_delimiter, -1, -1);
    }

    Status append_json(const char* data, size_t size, char row_delimiter, int32_t partition, int64_t offset) {
        bool need_meta = partition >= 0 && offset >= 0;
        // For efficiency reasons, simdjson requires a string with a few bytes (simdjson::SIMDJSON_PADDING) at the end.
        ASSIGN_OR_RETURN(auto buf, ByteBuffer::allocate_with_tracker(
                                           size + simdjson::SIMDJSON_PADDING,
                                           need_meta ? ByteBufferMetaType::KAFKA : ByteBufferMetaType::NONE));
        buf->put_bytes(data, size);
        buf->flip();
        if (need_meta) {
            KafkaByteBufferMeta* meta = static_cast<KafkaByteBufferMeta*>(buf->meta());
            meta->set_partition(partition);
            meta->set_offset(offset);
        }
        return append(std::move(buf));
    }
};

using PulsaConsumerPipe = KafkaConsumerPipe;

} // end namespace starrocks
