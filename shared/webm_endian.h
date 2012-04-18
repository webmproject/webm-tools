// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef SHARED_WEBM_ENDIAN_H_
#define SHARED_WEBM_ENDIAN_H_

#include "webm_tools_types.h"

namespace webm_tools {

// Swaps unsigned 64 bit values to big endian if needed. Returns |value| if
// architecture is big endian. Returns little endian value if architecture is
// little endian. Returns 0 otherwise.
uint64 host_to_bigendian(uint64 value);

// Swaps unsigned 64 bit values to little endian if needed. Returns |value| if
// architecture is big endian. Returns little endian value if architecture is
// little endian. Returns 0 otherwise.
uint64 bigendian_to_host(uint64 value);

}  // namespace webm_tools

#endif  // SHARED_WEBM_ENDIAN_H_
