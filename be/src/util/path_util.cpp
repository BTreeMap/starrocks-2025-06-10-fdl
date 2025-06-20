#include "util/path_util.h"

#include <memory>
// Use the POSIX version of basename(3). See `man 3 basename`
#include <libgen.h>

#include "gutil/strings/split.h"
#include "gutil/strings/stringpiece.h"
#include "gutil/strings/strip.h"

using std::string;
using std::vector;
using strings::SkipEmpty;
using strings::Split;

namespace starrocks::path_util {

const string kTmpInfix = ".starrockstmp";

std::string join_path_segments(const string& a, const string& b) {
    if (a.empty()) {
        return b;
    } else if (b.empty()) {
        return a;
    } else {
        return StripSuffixString(a, "/") + "/" + StripPrefixString(b, "/");
    }
}

std::vector<string> join_path_segments_v(const std::vector<string>& v, const string& s) {
    std::vector<string> out;
    out.reserve(v.size());
    for (const string& path : v) {
        out.emplace_back(join_path_segments(path, s));
    }
    return out;
}

std::vector<string> split_path(const string& path) {
    if (path.empty()) {
        return {};
    }
    std::vector<string> segments;
    if (path[0] == '/') {
        segments.emplace_back("/");
    }
    std::vector<StringPiece> pieces = Split(path, "/", SkipEmpty());
    for (const StringPiece& piece : pieces) {
        segments.emplace_back(piece.data(), piece.size());
    }
    return segments;
}

// strdup use malloc to obtain memory for the new string, it should be freed with free.
// but std::unique_ptr use delete to free memory by default, so it should specify free memory using free

std::string dir_name(const string& path) {
    std::vector<char> path_copy(path.c_str(), path.c_str() + path.size() + 1);
    return dirname(&path_copy[0]);
}

std::string base_name(const string& path) {
    std::vector<char> path_copy(path.c_str(), path.c_str() + path.size() + 1);
    return basename(&path_copy[0]);
}

std::string file_extension(const string& path) {
    string file_name = base_name(path);
    if (file_name == "." || file_name == ".." || file_name.find('.') == 0) {
        return "";
    }

    string::size_type pos = file_name.rfind('.');
    return pos == string::npos ? "" : file_name.substr(pos);
}

} // namespace starrocks::path_util
