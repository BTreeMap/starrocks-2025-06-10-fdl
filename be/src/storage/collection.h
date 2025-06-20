#pragma once

#include <iostream>

namespace starrocks {

// cpp type for ARRAY
struct Collection {
    // child column data
    void* data{nullptr};
    uint32_t length{0};
    // item has no null value if has_null is false.
    // item ```may``` has null value if has_null is true.
    bool has_null{false};
    // null bitmap
    uint8_t* null_signs{nullptr};

    Collection() = default;

    explicit Collection(uint32_t length) : length(length) {}

    Collection(void* data, size_t length) : data(data), length(static_cast<uint32_t>(length)) {}

    Collection(void* data, size_t length, uint8_t* null_signs)
            : data(data), length(static_cast<uint32_t>(length)), has_null(true), null_signs(null_signs) {}

    Collection(void* data, size_t length, bool has_null, uint8_t* null_signs)
            : data(data), length(static_cast<uint32_t>(length)), has_null(has_null), null_signs(null_signs) {}

    bool is_null_at(uint32_t index) const { return this->has_null && this->null_signs[index]; }

    bool operator==(const Collection& y) const;
    bool operator!=(const Collection& value) const;
    bool operator<(const Collection& value) const;
    bool operator<=(const Collection& value) const;
    bool operator>(const Collection& value) const;
    bool operator>=(const Collection& value) const;
    int32_t cmp(const Collection& other) const;
};

} // namespace starrocks
