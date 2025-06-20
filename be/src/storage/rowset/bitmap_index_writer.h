#pragma once

#include <cstddef>
#include <memory>

#include "common/status.h"
#include "gen_cpp/segment.pb.h"
#include "gutil/macros.h"

namespace starrocks {

class TypeInfo;
using TypeInfoPtr = std::shared_ptr<TypeInfo>;

class WritableFile;

class BitmapIndexWriter {
public:
    static Status create(const TypeInfoPtr& type_info, std::unique_ptr<BitmapIndexWriter>* res);

    BitmapIndexWriter() = default;
    virtual ~BitmapIndexWriter() = default;

    virtual void add_values(const void* values, size_t count) = 0;

    virtual void add_nulls(uint32_t count) = 0;

    virtual Status finish(WritableFile* file, ColumnIndexMetaPB* index_meta) = 0;

    virtual uint64_t size() const = 0;

private:
    BitmapIndexWriter(const BitmapIndexWriter&) = delete;
    const BitmapIndexWriter& operator=(const BitmapIndexWriter&) = delete;
};

} // namespace starrocks
