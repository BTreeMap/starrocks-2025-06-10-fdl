#pragma once
// This is a wrapper around the glog header.  When we are compiling to IR,
// we don't want to pull in the glog headers.  Pulling them in causes linking
// issues when we try to dynamically link the codegen'd functions.
#ifdef IR_COMPILE
#include <iostream>
#define DCHECK(condition) \
    while (false) std::cout
#define DCHECK_EQ(a, b) \
    while (false) std::cout
#define DCHECK_NE(a, b) \
    while (false) std::cout
#define DCHECK_GT(a, b) \
    while (false) std::cout
#define DCHECK_LT(a, b) \
    while (false) std::cout
#define DCHECK_GE(a, b) \
    while (false) std::cout
#define DCHECK_LE(a, b) \
    while (false) std::cout
// Similar to how glog defines DCHECK for release.
#define LOG(level) \
    while (false) std::cout
#define VLOG(level) \
    while (false) std::cout
#else
// GLOG defines this based on the system but doesn't check if it's already
// been defined.  undef it first to avoid warnings.
// glog MUST be included before gflags.  Instead of including them,
// our files should include this file instead.
#undef _XOPEN_SOURCE
// This is including a glog internal file.  We want this to expose the
// function to get the stack trace.
#include <glog/logging.h>
#undef MutexLock
#endif

// Define VLOG levels.  We want display per-row info less than per-file which
// is less than per-query.  For now per-connection is the same as per-query.
#define VLOG_CONNECTION VLOG(2)
#define VLOG_RPC VLOG(8)
#define VLOG_QUERY VLOG(2)
#define VLOG_FILE VLOG(2)
#define VLOG_OPERATOR VLOG(3)
#define VLOG_ROW VLOG(10)
#define VLOG_PROGRESS VLOG(2)
#define VLOG_CACHE VLOG(1)

#define VLOG_CONNECTION_IS_ON VLOG_IS_ON(1)
#define VLOG_RPC_IS_ON VLOG_IS_ON(2)
#define VLOG_QUERY_IS_ON VLOG_IS_ON(1)
#define VLOG_FILE_IS_ON VLOG_IS_ON(2)
#define VLOG_OPERATOR_IS_ON VLOG_IS_ON(3)
#define VLOG_ROW_IS_ON VLOG_IS_ON(10)
#define VLOG_PROGRESS_IS_ON VLOG_IS_ON(2)

namespace starrocks {
class TUniqueId;
}

// Print log with query id.
#define QUERY_LOG(level) LOG(level) << "[" << tls_thread_status.query_id() << "] "
#define QUERY_LOG_IF(level, cond) LOG_IF(level, cond) << "[" << tls_thread_status.query_id() << "] "
