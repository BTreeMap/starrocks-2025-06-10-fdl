#pragma once

#include <cstdint>

#include "runtime/decimalv2_value.h"
#include "storage/collection.h"
#include "storage/olap_common.h"
#include "types/logical_type.h"
#include "util/slice.h"

namespace starrocks {

// CppTypeTraits:
// Infer on-disk type(CppType) from LogicalType
template <LogicalType field_type>
struct CppTypeTraits {};

template <>
struct CppTypeTraits<TYPE_BOOLEAN> {
    using CppType = bool;
    using UnsignedCppType = bool;
};

template <>
struct CppTypeTraits<TYPE_NONE> {
    using CppType = bool;
    using UnsignedCppType = bool;
};

template <>
struct CppTypeTraits<TYPE_TINYINT> {
    using CppType = int8_t;
    using UnsignedCppType = uint8_t;
};
template <>
struct CppTypeTraits<TYPE_UNSIGNED_TINYINT> {
    using CppType = uint8_t;
    using UnsignedCppType = uint8_t;
};
template <>
struct CppTypeTraits<TYPE_SMALLINT> {
    using CppType = int16_t;
    using UnsignedCppType = uint16_t;
};
template <>
struct CppTypeTraits<TYPE_UNSIGNED_SMALLINT> {
    using CppType = uint16_t;
    using UnsignedCppType = uint16_t;
};
template <>
struct CppTypeTraits<TYPE_INT> {
    using CppType = int32_t;
    using UnsignedCppType = uint32_t;
};
template <>
struct CppTypeTraits<TYPE_UNSIGNED_INT> {
    using CppType = uint32_t;
    using UnsignedCppType = uint32_t;
};
template <>
struct CppTypeTraits<TYPE_BIGINT> {
    using CppType = int64_t;
    using UnsignedCppType = uint64_t;
};
template <>
struct CppTypeTraits<TYPE_UNSIGNED_BIGINT> {
    using CppType = uint64_t;
    using UnsignedCppType = uint64_t;
};
template <>
struct CppTypeTraits<TYPE_LARGEINT> {
    using CppType = int128_t;
    using UnsignedCppType = uint128_t;
};
template <>
struct CppTypeTraits<TYPE_FLOAT> {
    using CppType = float;
};
template <>
struct CppTypeTraits<TYPE_DOUBLE> {
    using CppType = double;
};
template <>
struct CppTypeTraits<TYPE_DECIMAL> {
    using CppType = decimal12_t;
    using UnsignedCppType = decimal12_t;
};
template <>
struct CppTypeTraits<TYPE_DECIMALV2> {
    using CppType = DecimalV2Value;
    using UnsignedCppType = DecimalV2Value;
};

template <>
struct CppTypeTraits<TYPE_DECIMAL32> {
    using CppType = int32_t;
    using UnsignedCppType = uint32_t;
};

template <>
struct CppTypeTraits<TYPE_DECIMAL64> {
    using CppType = int64_t;
    using UnsignedCppType = uint64_t;
};

template <>
struct CppTypeTraits<TYPE_DECIMAL128> {
    using CppType = int128_t;
    using UnsignedCppType = uint128_t;
};

template <>
struct CppTypeTraits<TYPE_DATE_V1> {
    using CppType = uint24_t;
    using UnsignedCppType = uint24_t;
};
template <>
struct CppTypeTraits<TYPE_DATE> {
    using CppType = int32_t;
    using UnsignedCppType = uint32_t;
};
template <>
struct CppTypeTraits<TYPE_DATETIME_V1> {
    using CppType = int64_t;
    using UnsignedCppType = uint64_t;
};
template <>
struct CppTypeTraits<TYPE_DATETIME> {
    using CppType = int64_t;
    using UnsignedCppType = uint64_t;
};
template <>
struct CppTypeTraits<TYPE_CHAR> {
    using CppType = Slice;
};
template <>
struct CppTypeTraits<TYPE_VARCHAR> {
    using CppType = Slice;
};
template <>
struct CppTypeTraits<TYPE_HLL> {
    using CppType = Slice;
};
template <>
struct CppTypeTraits<TYPE_OBJECT> {
    using CppType = Slice;
};
template <>
struct CppTypeTraits<TYPE_PERCENTILE> {
    using CppType = Slice;
};
template <>
struct CppTypeTraits<TYPE_JSON> {
    using CppType = Slice;
};
template <>
struct CppTypeTraits<TYPE_VARBINARY> {
    using CppType = Slice;
};
template <>
struct CppTypeTraits<TYPE_ARRAY> {
    using CppType = Collection;
};

// Instantiate this template to get static access to the type traits.
template <LogicalType field_type>
struct TypeTraits {
    using CppType = typename CppTypeTraits<field_type>::CppType;

    static const LogicalType type = field_type;
    static const int32_t size = sizeof(CppType);
};

} // namespace starrocks
