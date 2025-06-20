#pragma once

#include <cstdint>
#include <string>

#include "gen_cpp/segment.pb.h"
#include "util/coding.h"
#include "util/faststring.h"

namespace starrocks {

class PagePointer {
public:
    uint64_t offset{0};
    uint32_t size{0};

    PagePointer() = default;
    PagePointer(uint64_t offset_, uint32_t size_) : offset(offset_), size(size_) {}
    PagePointer(const PagePointerPB& from) : offset(from.offset()), size(from.size()) {}

    void reset() {
        offset = 0;
        size = 0;
    }

    void to_proto(PagePointerPB* to) {
        to->set_offset(offset);
        to->set_size(size);
    }

    const uint8_t* decode_from(const uint8_t* data, const uint8_t* limit) {
        data = decode_varint64_ptr(data, limit, &offset);
        if (data == nullptr) {
            return nullptr;
        }
        return decode_varint32_ptr(data, limit, &size);
    }

    bool decode_from(Slice* input) {
        bool result = get_varint64(input, &offset);
        if (!result) {
            return false;
        }
        return get_varint32(input, &size);
    }

    void encode_to(faststring* dst) const { put_varint64_varint32(dst, offset, size); }

    void encode_to(std::string* dst) const { put_varint64_varint32(dst, offset, size); }

    bool operator==(const PagePointer& other) const { return offset == other.offset && size == other.size; }

    bool operator!=(const PagePointer& other) const { return !(*this == other); }
};

} // namespace starrocks
