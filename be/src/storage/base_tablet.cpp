#include "storage/base_tablet.h"

#include <fmt/format.h>

#include "storage/data_dir.h"
#include "util/path_util.h"

namespace starrocks {

BaseTablet::BaseTablet(const TabletMetaSharedPtr& tablet_meta, DataDir* data_dir)
        : _state(tablet_meta->tablet_state()), _tablet_meta(tablet_meta), _data_dir(data_dir) {
    _gen_tablet_path();
}

Status BaseTablet::set_tablet_state(TabletState state) {
    if (_tablet_meta->tablet_state() == TABLET_SHUTDOWN && state != TABLET_SHUTDOWN) {
        LOG(WARNING) << "could not change tablet state from shutdown to " << state;
        return Status::InvalidArgument(fmt::format("Change tablet state from SHUTODWN to {} is forbidden", state));
    }
    _tablet_meta->set_tablet_state(state);
    _state = state;
    if (state == TABLET_SHUTDOWN) {
        on_shutdown();
    }
    return Status::OK();
}

void BaseTablet::_gen_tablet_path() {
    if (_data_dir != nullptr) {
        std::string path = _data_dir->path() + DATA_PREFIX;
        path = path_util::join_path_segments(path, std::to_string(_tablet_meta->shard_id()));
        path = path_util::join_path_segments(path, std::to_string(_tablet_meta->tablet_id()));
        path = path_util::join_path_segments(path, std::to_string(_tablet_meta->schema_hash()));
        _tablet_path = path;
    }
}

std::string BaseTablet::tablet_id_path() const {
    return _tablet_path.substr(0, _tablet_path.rfind('/'));
}

} /* namespace starrocks */
