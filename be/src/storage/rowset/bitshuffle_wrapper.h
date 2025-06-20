#pragma once

#include <cstddef>
#include <cstdint>

// This namespace has wrappers for the Bitshuffle library which do runtime dispatch to
// either AVX2-accelerated or regular SSE2 implementations based on the available CPU.
namespace starrocks::bitshuffle {

// See <bitshuffle.h> for documentation on these functions.
size_t compress_lz4_bound(size_t size, size_t elem_size, size_t block_size);
int64_t compress_lz4(void* in, void* out, size_t size, size_t elem_size, size_t block_size);
int64_t decompress_lz4(void* in, void* out, size_t size, size_t elem_size, size_t block_size);

} // namespace starrocks::bitshuffle
