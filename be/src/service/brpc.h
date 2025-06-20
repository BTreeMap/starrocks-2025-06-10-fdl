#pragma once

// This file is used to fixed macro conflict between butil and gutil
// all header need by brpc is contain in this file.
// include this file instead of include <brpc/xxx.h>
// and this file must put the first include in soure file

#include "gutil/macros.h"
// Macros in the guti/macros.h, use butil's define
#ifdef DISALLOW_IMPLICIT_CONSTRUCTORS
#undef DISALLOW_IMPLICIT_CONSTRUCTORS
#endif

#undef OVERRIDE
#undef FINAL

// use be/src/gutil/integral_types.h override butil/basictypes.h
#include "gutil/integral_types.h"
#ifdef BASE_INTEGRAL_TYPES_H_
#define BUTIL_BASICTYPES_H_
#endif

#ifdef DEBUG_MODE
#undef DEBUG_MODE
#endif

#include <brpc/channel.h>
#include <brpc/closure_guard.h>
#include <brpc/controller.h>
#include <brpc/protocol.h>
#include <brpc/reloadable_flags.h>
#include <brpc/server.h>
#include <butil/containers/flat_map.h>
#include <butil/containers/flat_map_inl.h>
#include <butil/endpoint.h>
#include <butil/fd_utility.h>
#include <butil/macros.h>
#include <butil/strings/string_piece.h>

#include "common/compiler_util.h"
#include "common/config.h"

// ignore brpc overcrowded error
#define SET_IGNORE_OVERCROWDED(ctnl, module)          \
    if (config::brpc_##module##_ignore_overcrowded) { \
        ctnl.ignore_eovercrowded();                   \
    }
