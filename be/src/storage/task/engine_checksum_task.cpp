// Copyright 2021-present StarRocks, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file is based on code available under the Apache license here:
//   https://github.com/apache/incubator-doris/blob/master/be/src/olap/task/engine_checksum_task.cpp

// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "storage/task/engine_checksum_task.h"

#include <memory>

#include "runtime/current_thread.h"
#include "runtime/exec_env.h"
#include "storage/chunk_helper.h"
#include "storage/tablet_reader.h"
#include "util/defer_op.h"

namespace starrocks {

EngineChecksumTask::EngineChecksumTask(MemTracker* mem_tracker, TTabletId tablet_id, TVersion version,
                                       uint32_t* checksum)
        : _tablet_id(tablet_id), _version(version), _checksum(checksum) {
    _mem_tracker = std::make_unique<MemTracker>(-1, "checksum instance", mem_tracker);
}

Status EngineChecksumTask::execute() {
    SCOPED_THREAD_LOCAL_MEM_TRACKER_SETTER(_mem_tracker.get());

    return _compute_checksum();
}

Status EngineChecksumTask::_compute_checksum() {
    int64_t begin_us = butil::gettimeofday_us();
    VLOG(1) << "begin to process compute checksum."
            << "tablet_id=" << _tablet_id << ", version=" << _version;

    if (_checksum == nullptr) {
        LOG(WARNING) << "The input checksum is a null pointer";
        return Status::InternalError("The input checksum is a null pointer");
    }

    TabletSharedPtr tablet = StorageEngine::instance()->tablet_manager()->get_tablet(_tablet_id);
    if (tablet == nullptr) {
        LOG(WARNING) << "Not found tablet: " << _tablet_id;
        return Status::NotFound(fmt::format("Not found tablet: {}", _tablet_id));
    }

    std::vector<uint32_t> return_columns;
    auto tablet_schema = tablet->tablet_schema();

    size_t num_columns = tablet_schema->num_columns();
    for (size_t i = 0; i < num_columns; ++i) {
        const auto& column = tablet_schema->column(i);
        if (column.is_support_checksum()) {
            return_columns.push_back(i);
        }
    }

    Schema schema = ChunkHelper::convert_schema(tablet_schema, return_columns);

    TabletReader reader(tablet, Version(0, _version), schema);

    Status st = reader.prepare();
    if (!st.ok()) {
        LOG(WARNING) << "Failed to prepare tablet reader. tablet=" << tablet->full_name()
                     << ", error:" << st.to_string();
        return st;
    }

    // Calculate the chunk size based on the memory limit of the current thread.
    int64_t chunk_rows_limit = 0;
    // Estimate the average row size of the tablet
    int64_t average_row_size = tablet->get_average_row_size();
    if (average_row_size != 0 && _mem_tracker->parent()->has_limit()) {
        // Limit the chunk size to avoid memory overflow
        chunk_rows_limit = _mem_tracker->parent()->limit() / average_row_size /
                           std::max(1, config::check_consistency_worker_count);
    } else {
        // no limitation for empty tablet
        chunk_rows_limit = config::vector_chunk_size;
    }

    TabletReaderParams reader_params;
    reader_params.reader_type = READER_CHECKSUM;
    reader_params.chunk_size = std::min((int64_t)config::vector_chunk_size, chunk_rows_limit + 1);

    st = reader.open(reader_params);
    if (!st.ok()) {
        LOG(WARNING) << "Failed to open tablet reader. tablet=" << tablet->full_name() << ", error:" << st.to_string();
        return st;
    }

    int64_t checksum = 0;

    auto chunk = ChunkHelper::new_chunk(schema, reader_params.chunk_size);
    st = reader.get_next(chunk.get());

    bool bg_worker_stopped = StorageEngine::instance()->bg_worker_stopped();
    while (st.ok() && !bg_worker_stopped) {
#ifndef BE_TEST
        st = _mem_tracker->check_mem_limit("ConsistencyCheck");
        if (!st.ok()) {
            LOG(WARNING) << "failed to finish compute checksum. " << st.message() << std::endl;
            return st;
        }
#endif

        size_t size = chunk->num_rows();
        for (auto& column : chunk->columns()) {
            checksum ^= column->xor_checksum(0, size);
        }
        chunk->reset();
        st = reader.get_next(chunk.get());
        bg_worker_stopped = StorageEngine::instance()->bg_worker_stopped();
    }

    if (bg_worker_stopped) {
        return Status::InternalError("Process is going to quit. The checksum calculation will stop.");
    }

    if (!st.is_end_of_file() && !st.ok()) {
        LOG(WARNING) << "Failed to do checksum. tablet=" << tablet->full_name() << ", error:=" << st.to_string();
        return st;
    }

    int64_t cost_time = butil::gettimeofday_us() - begin_us;

    if (cost_time >= 30000000 /** 30s **/) {
        LOG(INFO) << "success to finish compute checksum. checksum=" << checksum << ", tablet_id=" << _tablet_id
                  << ", version=" << _version << " , cost_time=" << cost_time << "us";
    }
    *_checksum = checksum;
    return Status::OK();
}

} // namespace starrocks
