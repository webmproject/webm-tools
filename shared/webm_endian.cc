// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webm_endian.h"

namespace webm_tools {

// Swaps unsigned 64 bit values to big endian if needed. Returns |value| if
// architecture is big endian. Returns little endian value if architecture is
// little endian. Returns 0 otherwise.
uint64 host_to_bigendian(uint64 value) {
  // Check endianness.
  union {
    uint64 val64;
    uint8 c[8];
  } check;
  check.val64 = 0x0123456789ABCDEFULL;

  // Check for big endian.
  if (check.c[7] == 0xEF)
    return value;

  // Check for not little endian.
  if (check.c[0] != 0xEF)
    return 0;

  return value << 56 |
         ((value << 40) & 0x00FF000000000000ULL) |
         ((value << 24) & 0x0000FF0000000000ULL) |
         ((value << 8) & 0x000000FF00000000ULL) |
         ((value >> 8) & 0x00000000FF000000ULL) |
         ((value >> 24) & 0x0000000000FF0000ULL) |
         ((value >> 40) & 0x000000000000FF00ULL) |
         value >> 56;
}

// Swaps unsigned 64 bit values to little endian if needed. Returns |value| if
// architecture is big endian. Returns little endian value if architecture is
// little endian. Returns 0 otherwise.
uint64 bigendian_to_host(uint64 value) {
  // Check endianness.
  union {
    uint64 val64;
    uint8 c[8];
  } check;
  check.val64 = 0x0123456789ABCDEFULL;

  // Check for big endian.
  if (check.c[7] == 0xEF)
    return value;

  // Check for not little endian.
  if (check.c[0] != 0xEF)
    return 0;

  return value << 56 |
         ((value << 40) & 0x00FF000000000000ULL) |
         ((value << 24) & 0x0000FF0000000000ULL) |
         ((value << 8) & 0x000000FF00000000ULL) |
         ((value >> 8) & 0x00000000FF000000ULL) |
         ((value >> 24) & 0x0000000000FF0000ULL) |
         ((value >> 40) & 0x000000000000FF00ULL) |
         value >> 56;
}

}  // namespace webm_tools
