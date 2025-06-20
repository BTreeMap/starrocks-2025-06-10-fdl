#include "util/trace_metrics.h"

#include <glog/logging.h>
#include <glog/stl_logging.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <mutex>
#include <ostream>
#include <string>

#include "util/debug/leakcheck_disabler.h"

using std::string;

namespace starrocks {

// Make glog's STL-compatible operators visible inside this namespace.
using ::operator<<;

namespace {

static SpinLock g_intern_map_lock;
typedef std::map<string, const char*> InternMap;
static InternMap* g_intern_map;

} // anonymous namespace

const char* TraceMetrics::InternName(const string& name) {
    DCHECK(std::all_of(name.begin(), name.end(), [](char c) { return isprint(c); })) << "not printable: " << name;
    [[maybe_unused]] debug::ScopedLeakCheckDisabler no_leakcheck;
    std::lock_guard<SpinLock> l(g_intern_map_lock);
    if (g_intern_map == nullptr) {
        g_intern_map = new InternMap();
    }

    auto it = g_intern_map->find(name);
    if (it != g_intern_map->end()) {
        return it->second;
    }

    const char* dup = strdup(name.c_str());
    (*g_intern_map)[name] = dup;

    // We don't expect this map to grow large.
    DCHECK_LT(g_intern_map->size(), 100) << "Too many interned strings: " << *g_intern_map;

    return dup;
}

} // namespace starrocks
