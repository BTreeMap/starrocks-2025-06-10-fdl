#pragma once

#include <cstddef>
#include <memory>

#include "common/status.h"
#include "gen_cpp/segment.pb.h"
#include "gutil/macros.h"

namespace starrocks {

class TypeInfo;
using TypeInfoPtr = std::shared_ptr<TypeInfo>;

struct BloomFilterOptions;
class WritableFile;

class BloomFilterIndexWriter {
public:
    static Status create(const BloomFilterOptions& bf_options, const TypeInfoPtr& typeinfo,
                         std::unique_ptr<BloomFilterIndexWriter>* res);

    BloomFilterIndexWriter() = default;
    virtual ~BloomFilterIndexWriter() = default;

    virtual void add_values(const void* values, size_t count) = 0;

    virtual void add_nulls(uint32_t count) = 0;

    virtual Status flush() = 0;

    virtual Status finish(WritableFile* wfile, ColumnIndexMetaPB* index_meta) = 0;

    virtual uint64_t size() = 0;

private:
    BloomFilterIndexWriter(const BloomFilterIndexWriter&) = delete;
    const BloomFilterIndexWriter& operator=(const BloomFilterIndexWriter&) = delete;
};

} // namespace starrocks
