#pragma once

#include "gen_cpp/olap_file.pb.h"
#include "storage/data_dir.h"
#include "storage/rowset/rowset.h"

namespace starrocks {

class RowsetWriter;
class RowsetWriterContext;

class RowsetFactory {
public:
    // return OK on success and set inited rowset in `*rowset`.
    // return error if failed to create or init rowset.
    static Status create_rowset(const TabletSchemaCSPtr& schema, const std::string& rowset_path,
                                const RowsetMetaSharedPtr& rowset_meta, RowsetSharedPtr* rowset);

    // create and init rowset writer.
    // return OK on success and set `*output` to inited rowset writer.
    // return error if failed
    static Status create_rowset_writer(const RowsetWriterContext& context, std::unique_ptr<RowsetWriter>* output);
};

} // namespace starrocks
