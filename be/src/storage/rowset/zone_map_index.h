#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/status.h"
#include "gen_cpp/segment.pb.h"
#include "runtime/mem_pool.h"
#include "runtime/mem_tracker.h"
#include "storage/rowset/binary_plain_page.h"
#include "util/once.h"
#include "util/slice.h"

namespace starrocks {

class FileSystem;
class WritableFile;

// Zone map index is represented by an IndexedColumn with ordinal index.
// The IndexedColumn stores serialized ZoneMapPB for each data page.
// It also create and store the segment-level zone map in the index meta so that
// reader can prune an entire segment without reading pages.
class ZoneMapIndexWriter {
public:
    static std::unique_ptr<ZoneMapIndexWriter> create(TypeInfo* type_info);

    virtual ~ZoneMapIndexWriter() = default;

    virtual void add_values(const void* values, size_t count) = 0;

    virtual void add_nulls(uint32_t count) = 0;

    // mark the end of one data page so that we can finalize the corresponding zone map
    virtual Status flush() = 0;

    virtual Status finish(WritableFile* wfile, ColumnIndexMetaPB* index_meta) = 0;

    virtual uint64_t size() const = 0;
};

class ZoneMapIndexReader {
public:
    ZoneMapIndexReader();
    ~ZoneMapIndexReader();

    // load all page zone maps into memory.
    //
    // Multiple callers may call this method concurrently, but only the first one
    // can load the data, the others will wait until the first one finished loading
    // data.
    //
    // Return true if the index data was successfully loaded by the caller, false if
    // the data was loaded by another caller.
    StatusOr<bool> load(const IndexReadOptions& opts, const ZoneMapIndexPB& meta);

    // REQUIRES: the index data has been successfully `load()`ed into memory.
    const std::vector<ZoneMapPB>& page_zone_maps() const { return _page_zone_maps; }

    // REQUIRES: the index data has been successfully `load()`ed into memory.
    int32_t num_pages() const { return static_cast<int32_t>(_page_zone_maps.size()); }

    bool loaded() const { return invoked(_load_once); }

    size_t mem_usage() const;

private:
    void _reset() { std::vector<ZoneMapPB>{}.swap(_page_zone_maps); }

    Status _do_load(const IndexReadOptions& opts, const ZoneMapIndexPB& meta);

    OnceFlag _load_once;
    std::vector<ZoneMapPB> _page_zone_maps;
};

} // namespace starrocks
