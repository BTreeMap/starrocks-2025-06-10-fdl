#pragma once

#include <cfloat>
#include <cstdint>

namespace starrocks {

typedef unsigned char uchar;

#define int1store(T, A) *((uint8_t*)(T)) = (uint8_t)(A)
#define int2store(T, A) *((uint16_t*)(T)) = (uint16_t)(A)
#define int3store(T, A)                           \
    do {                                          \
        *(T) = (uchar)((A));                      \
        *(T + 1) = (uchar)(((uint32_t)(A) >> 8)); \
        *(T + 2) = (uchar)(((A) >> 16));          \
    } while (0)
#define int4store(T, A) *((uint32_t*)(T)) = (uint32_t)(A)
#define int8store(T, A) *((int64_t*)(T)) = (uint64_t)(A)
#define float4store(T, A) *((float*)(T)) = (float)(A)
#define float8store(T, A) *((double*)(T)) = (double)(A)
#define MAX_TINYINT_WIDTH 3  /* Max width for a TINY w.o. sign */
#define MAX_SMALLINT_WIDTH 5 /* Max width for a SHORT w.o. sign */
#define MAX_INT_WIDTH 10     /* Max width for a LONG w.o. sign */
#define MAX_BIGINT_WIDTH 20  /* Max width for a LONGLONG */

/* -[digits].E+## */
#define MAX_FLOAT_STR_LENGTH 24 // see gutil/strings/numbers.h kFloatToBufferSize
/* -[digits].E+### */
#define MAX_DOUBLE_STR_LENGTH 32 // see gutil/strings/numbers.h kDoubleToBufferSize

} // namespace starrocks
