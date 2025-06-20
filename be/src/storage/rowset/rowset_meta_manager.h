#pragma once

#include <string>
#include <string_view>

#include "storage/rowset/rowset_meta.h"

namespace starrocks {

class KVStore;

// Helper class for managing rowset meta of one root path.
class RowsetMetaManager {
public:
    static bool check_rowset_meta(KVStore* meta, const TabletUid& tablet_uid, const RowsetId& rowset_id);

    static Status save(KVStore* meta, const TabletUid& tablet_uid, const RowsetMetaPB& rowset_meta_pb);

    static Status flush(KVStore* meta);

    static Status remove(KVStore* meta, const TabletUid& tablet_uid, const RowsetId& rowset_id);

    static std::string get_rowset_meta_key(const TabletUid& tablet_uid, const RowsetId& rowset_id);

    static Status traverse_rowset_metas(
            KVStore* meta, std::function<bool(const TabletUid&, const RowsetId&, std::string_view)> const& func);

    static Status get_rowset_meta_value(KVStore* meta, const TabletUid& tablet_uid, const RowsetId& rowset_id,
                                        std::string* value);
};

} // namespace starrocks
