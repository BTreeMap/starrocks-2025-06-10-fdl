#pragma once

#include <map>
#include <memory>

#include "common/status.h"
#include "gen_cpp/segment.pb.h"
#include "storage/rowset/common.h"
#include "storage/rowset/indexed_column_reader.h"
#include "util/once.h"

namespace starrocks {

class TypeInfo;

class BloomFilterIndexIterator;
class FileSystem;
class IndexedColumnReader;
class IndexedColumnIterator;
class BloomFilter;

class BloomFilterIndexReader {
    friend class BloomFilterIndexIterator;

public:
    BloomFilterIndexReader();
    ~BloomFilterIndexReader();

    // Multiple callers may call this method concurrently, but only the first one
    // can load the data, the others will wait until the first one finished loading
    // data.
    //
    // Return true if the index data was successfully loaded by the caller, false if
    // the data was loaded by another caller.
    StatusOr<bool> load(const IndexReadOptions& opts, const BloomFilterIndexPB& meta);

    // create a new column iterator.
    // REQUIRES: the index data has been successfully `load()`ed into memory.
    Status new_iterator(const IndexReadOptions& opts, std::unique_ptr<BloomFilterIndexIterator>* iterator);

    const TypeInfoPtr& type_info() const { return _typeinfo; }

    bool loaded() const { return invoked(_load_once); }

    size_t mem_usage() const {
        size_t size = sizeof(BloomFilterIndexReader);
        if (_bloom_filter_reader != nullptr) {
            size += _bloom_filter_reader->mem_usage();
        }
        return size;
    }

private:
    Status _do_load(const IndexReadOptions& opts, const BloomFilterIndexPB& meta);

    void _reset();

    OnceFlag _load_once;
    TypeInfoPtr _typeinfo;
    BloomFilterAlgorithmPB _algorithm = BLOCK_BLOOM_FILTER;
    HashStrategyPB _hash_strategy = HASH_MURMUR3_X64_64;
    std::unique_ptr<IndexedColumnReader> _bloom_filter_reader;
};

class BloomFilterIndexIterator {
    friend class BloomFilterIndexReader;

public:
    // Read bloom filter at the given ordinal into `bf`.
    Status read_bloom_filter(rowid_t ordinal, std::unique_ptr<BloomFilter>* bf);

private:
    BloomFilterIndexIterator(BloomFilterIndexReader* reader, std::unique_ptr<IndexedColumnIterator> bf_iter)
            : _reader(reader), _bloom_filter_iter(std::move(bf_iter)) {}

    BloomFilterIndexReader* _reader;
    std::unique_ptr<IndexedColumnIterator> _bloom_filter_iter;
};

} // namespace starrocks
