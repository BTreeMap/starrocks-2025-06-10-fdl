#pragma once

#include "common/status.h" // for Status
#include "gen_cpp/segment.pb.h"
#include "storage/range.h"
#include "storage/rowset/page_pointer.h"
#include "types/timestamp_value.h"

namespace starrocks {
class Column;
}

namespace starrocks {

// PageDecoder is used to decode page.
class PageDecoder {
public:
    PageDecoder() = default;

    virtual ~PageDecoder() = default;

    // Call this to do some preparation for decoder.
    // eg: parse data page header
    virtual Status init() = 0;

    // Seek the decoder to the given positional index of the page.
    // For example, seek_to_position_in_page(0) seeks to the first
    // stored entry.
    //
    // It is an error to call this with a value larger than Count().
    // Doing so has undefined results.
    virtual Status seek_to_position_in_page(uint32_t pos) = 0;

    // Seek the decoder to the given value in the page, or the
    // lowest value which is greater than the given value.
    //
    // If the decoder was able to locate an exact match, then
    // sets *exact_match to true. Otherwise sets *exact_match to
    // false, to indicate that the seeked value is _after_ the
    // requested value.
    //
    // If the given value is less than the lowest value in the page,
    // seeks to the start of the page. If it is higher than the highest
    // value in the page, then returns Status::NotFound
    //
    // This will only return valid results when the data page
    // consists of values in sorted order.
    virtual Status seek_at_or_after_value(const void* value, bool* exact_match) {
        return Status::NotSupported("seek_at_or_after_value"); // FIXME
    }

    virtual Status next_batch(size_t* n, Column* column) {
        return Status::NotSupported("vectorized not supported yet");
    }

    virtual Status next_batch(const SparseRange<>& range, Column* column) {
        return Status::NotSupported("PageDecoder Not Support");
    }

    // Return the number of elements in this page.
    virtual uint32_t count() const = 0;

    // Return the position within the page of the currently seeked
    // entry (ie the entry that will next be returned by next_vector())
    virtual uint32_t current_index() const = 0;

    // Return the the encoding type of current page.
    // normally, the encoding type is fixed for all data pages of a single column, but
    // dictionary encoded column is an exception: it first use dictionary encoding as
    // the codec algorithm then switched to plain encoding when the dictionary page is full.
    virtual EncodingTypePB encoding_type() const = 0;

    virtual Status next_dict_codes(size_t* n, Column* dst) {
        return Status::NotSupported("next_dict_codes() not supported");
    }

    virtual Status next_dict_codes(const SparseRange<>& range, Column* dst) {
        return Status::NotSupported("next_dict_codes() not supported");
    }

    virtual const PageDecoder* dict_page_decoder() const { return nullptr; }

private:
    PageDecoder(const PageDecoder&) = delete;
    const PageDecoder& operator=(const PageDecoder&) = delete;
};

} // namespace starrocks
