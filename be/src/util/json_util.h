#pragma once

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>

#include <string>
#include <vector>

#include "column/vectorized_fwd.h"
#include "common/status.h"
#include "util/pretty_printer.h"
#include "util/template_util.h"

namespace starrocks {

/// ToJsonValue() converts 'value' into a rapidjson::Value in 'out_val'. The type of
/// 'out_val' depends on the value of 'type'. If type != TUnit::NONE and 'value' is
/// numeric, 'value' will be set to a string which is the pretty-printed representation of
/// 'value' (e.g. conversion to MB etc). Otherwise the value is directly copied into
/// 'out_val'.
template <typename T>
ENABLE_IF_NOT_ARITHMETIC(T, void)
ToJsonValue(const T& value, const TUnit::type unit, rapidjson::Document* document, rapidjson::Value* out_val) {
    *out_val = value;
}

/// Specialisation for std::string which requires explicit use of 'document's allocator to
/// copy into out_val.
template <>
void ToJsonValue<std::string>(const std::string& value, const TUnit::type unit, rapidjson::Document* document,
                              rapidjson::Value* out_val);

/// Does pretty-printing if 'value' is numeric, and type is not NONE, otherwise constructs
/// a json object containing 'value' as a literal.
template <typename T>
ENABLE_IF_ARITHMETIC(T, void)
ToJsonValue(const T& value, const TUnit::type unit, rapidjson::Document* document, rapidjson::Value* out_val) {
    if (unit != TUnit::NONE) {
        const std::string& s = PrettyPrinter::print(value, unit);
        ToJsonValue(s, unit, document, out_val);
    } else {
        *out_val = value;
    }
}

std::string to_json(const Status& status);

std::string to_json(const std::map<std::string, std::map<std::string, std::string>>& value);

Status from_json(const std::string& json_value, std::map<std::string, std::map<std::string, std::string>>* map_result);
} // namespace starrocks
