#pragma once

#include "geo/geo_common.h"

namespace starrocks {
class GeoShape;
}

typedef void* yyscan_t;
class WktParseContext {
public:
    yyscan_t scaninfo{};
    starrocks::GeoShape* shape = nullptr;
    starrocks::GeoParseStatus parse_status = starrocks::GEO_PARSE_OK;
};
