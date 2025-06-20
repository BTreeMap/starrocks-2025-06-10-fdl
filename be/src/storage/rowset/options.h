#pragma once

#include <cstddef>

#include "io/seekable_input_stream.h"
#include "storage/rowset/page_handle.h"
namespace starrocks {

class FileSystem;
class RandomAccessFile;

static const uint32_t DEFAULT_PAGE_SIZE = 1024 * 1024; // default size: 1M

class PageBuilderOptions {
public:
    uint32_t data_page_size = DEFAULT_PAGE_SIZE;

    uint32_t dict_page_size = config::dictionary_page_size;
};

class IndexReadOptions {
public:
    bool use_page_cache = false;
    // for lake tablet
    LakeIOOptions lake_io_opts{.fill_data_cache = true};

    //RandomAccessFile* read_file = nullptr;
    io::SeekableInputStream* read_file = nullptr;
    OlapReaderStatistics* stats = nullptr;
};

} // namespace starrocks
