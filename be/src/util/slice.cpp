#include "util/slice.h"

#include "util/faststring.h"

namespace starrocks {

static const uint16_t _SLICE_MAX_LENGTH = 65535;
static char _slice_max_value_data[_SLICE_MAX_LENGTH];
static Slice _slice_max_value;
static Slice _slice_min_value;

class SliceInit {
public:
    SliceInit() { Slice::init(); }
};

static SliceInit _slice_init;

// NOTE(zc): we define this function here to make compile work.
Slice::Slice(const faststring& s)
        : // NOLINT(runtime/explicit)
          data((char*)(s.data())),
          size(s.size()) {}

void Slice::init() {
    memset(_slice_max_value_data, 0xff, sizeof(_slice_max_value_data));
    _slice_max_value = Slice(_slice_max_value_data, sizeof(_slice_max_value_data));
    _slice_min_value = Slice(const_cast<char*>(""), 0);
}

const Slice& Slice::max_value() {
    return _slice_max_value;
}

const Slice& Slice::min_value() {
    return _slice_min_value;
}

} // namespace starrocks
