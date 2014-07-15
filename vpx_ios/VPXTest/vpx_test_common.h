// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef VPXTest_VPXTest_vpx_test_common_h_
#define VPXTest_VPXTest_vpx_test_common_h_

namespace VpxTest {

enum VpxCodec {
  UNKNOWN,
  VP8,
  VP9,
};

struct VpxFormat {
  VpxFormat() : codec(UNKNOWN), width(0), height(0) {}
  VpxCodec codec;
  int width;
  int height;
};

}  // namespace VpxTest

#endif  // VPXTest_VPXTest_vpx_test_common_h_