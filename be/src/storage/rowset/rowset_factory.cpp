#include "storage/rowset/rowset_factory.h"

#include <memory>

#include "gen_cpp/olap_file.pb.h"
#include "rowset.h"
#include "runtime/exec_env.h"
#include "storage/rowset/horizontal_update_rowset_writer.h"
#include "storage/rowset/rowset_writer.h"

namespace starrocks {

Status RowsetFactory::create_rowset(const TabletSchemaCSPtr& schema, const std::string& rowset_path,
                                    const RowsetMetaSharedPtr& rowset_meta, RowsetSharedPtr* rowset) {
    *rowset = Rowset::create(schema, rowset_path, rowset_meta);
    RETURN_IF_ERROR((*rowset)->init());
    return Status::OK();
}

Status RowsetFactory::create_rowset_writer(const RowsetWriterContext& context, std::unique_ptr<RowsetWriter>* output) {
    if (context.writer_type == kHorizontal) {
        if (context.partial_update_mode == PartialUpdateMode::COLUMN_UPSERT_MODE ||
            context.partial_update_mode == PartialUpdateMode::COLUMN_UPDATE_MODE) {
            // rowset writer for partial update in column mode
            *output = std::make_unique<HorizontalUpdateRowsetWriter>(context);
        } else {
            *output = std::make_unique<HorizontalRowsetWriter>(context);
        }
    } else {
        DCHECK(context.writer_type == kVertical);
        *output = std::make_unique<VerticalRowsetWriter>(context);
    }
    return (*output)->init();
}

} // namespace starrocks
