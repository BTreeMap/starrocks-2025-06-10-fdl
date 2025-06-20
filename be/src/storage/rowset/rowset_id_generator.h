#pragma once

#include <mutex>

#include "storage/olap_common.h"
#include "storage/olap_define.h"

namespace starrocks {

// all implementations must be thread-safe
class RowsetIdGenerator {
public:
    RowsetIdGenerator() = default;
    virtual ~RowsetIdGenerator() = default;

    // generate and return the next global unique rowset id
    virtual RowsetId next_id() = 0;

    // check whether the rowset id is useful or validate
    // for example, during gc logic, gc thread finds a file
    // and it could not find it under rowset list. but it maybe in use
    // during load procedure. Gc thread will check it using this method.
    virtual bool id_in_use(const RowsetId& rowset_id) const = 0;

    // remove the rowsetid from useful rowsetid list.
    virtual void release_id(const RowsetId& rowset_id) = 0;
}; // RowsetIdGenerator

} // namespace starrocks
