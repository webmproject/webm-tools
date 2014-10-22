// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef VPX_IOS_VPXEXAMPLE_VPX_FRAME_PARSER_H_
#define VPX_IOS_VPXEXAMPLE_VPX_FRAME_PARSER_H_

#include <string>
#include <vector>

#include "./vpx_test_common.h"

namespace VpxExample {

// A generic interface for reading frames of VP8 or VP9 video from a data source
// that supports synchronous reads.
class VpxFrameParserInterface {
 public:

  // VPX frame data ready for decoding by Libvpx.
  struct VpxFrame {
    VpxFrame() : data(NULL), length(0), timestamp(0) {}

    // Existing, user owned, vector<uint8_t>* used by the
    // VpxFrameParserInterface implementation to store compressed VPx bitstream
    // data.
    std::vector<uint8_t> *data;

    // Length of VPx bitstream data contained in |data|.
    uint32_t length;

    // Timestamp of |data|.
    int64_t timestamp;

    // Timebase of |timestamp|.
    VpxTimeBase timebase;
  };

  virtual ~VpxFrameParserInterface() {}

  // Implementations return true when the container file contains a VP8 or VP9
  // bitstream.
  virtual bool HasVpxFrames(const std::string &file_path,
                            VpxFormat *vpx_format) = 0;

  // Implementations read frames into the std::vector<uint8_t>* stored within
  // the VpxFrame*. The user owns the vector<uint8_t>. The implementation MUST
  // return false when the vector<uint8_t>* is NULL.
  virtual bool ReadFrame(VpxFrame *frame) = 0;
};

}  // namespace VpxExample

#endif  // VPX_IOS_VPXEXAMPLE_VPX_FRAME_PARSER_H_

