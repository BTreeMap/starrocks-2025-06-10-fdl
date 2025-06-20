#pragma once

#include <utility>

#include "gutil/macros.h"

namespace starrocks {

// This class is used to defer a function when this object is deconstruct
template <class DeferFunction>
class DeferOp {
public:
    explicit DeferOp(DeferFunction func) : _func(std::move(func)) {}

    ~DeferOp() noexcept { (void)_func(); }

    DISALLOW_COPY_AND_MOVE(DeferOp);

private:
    DeferFunction _func;
};

template <class DeferFunction>
class CancelableDefer {
public:
    CancelableDefer(DeferFunction func) : _func(std::move(func)) {}
    ~CancelableDefer() noexcept {
        if (!_cancel) {
            (void)_func();
        }
    }
    void cancel() { _cancel = true; }
    DISALLOW_COPY_AND_MOVE(CancelableDefer);

private:
    bool _cancel{};
    DeferFunction _func;
};

} // namespace starrocks
