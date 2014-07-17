// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef VPXTEST_VPXTEST_VPX_FRAME_PARSER_H_
#define VPXTEST_VPXTEST_VPX_FRAME_PARSER_H_

#include <vector>

#include "vpx_test_common.h"

namespace VpxTest {

class VpxFrameParser {
 public:
  virtual bool HasVpxFrames(const std::string &file_path,
                            VpxFormat *vpx_format) = 0;
  virtual bool ReadFrame(std::vector<uint8_t> *frame,
                         uint32_t *frame_length) = 0;
};

}  // namespace VpxTest

#endif  // VPXTEST_VPXTEST_VPX_FRAME_PARSER_H_
