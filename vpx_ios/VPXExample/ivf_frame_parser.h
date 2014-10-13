// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef VPX_IOS_VPXEXAMPLE_IVF_FRAME_PARSER_H_
#define VPX_IOS_VPXEXAMPLE_IVF_FRAME_PARSER_H_

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "./vpx_frame_parser.h"
#include "./vpx_test_common.h"

namespace VpxExample {

// Returns frames from the IVF file when the IVF file contains a four character
// code known to be a VPx format.
class IvfFrameParser : public VpxFrameParserInterface {
 public:
  IvfFrameParser() : rate_(0), scale_(0), frame_count_(0) {}
  virtual ~IvfFrameParser() {}
  bool HasVpxFrames(const std::string &file_path,
                    VpxFormat *vpx_format) override;
  bool ReadFrame(std::vector<uint8_t> *frame, uint32_t *frame_length) override;

 private:
  struct FileCloser {
    void operator()(FILE* file);
  };

  std::unique_ptr<FILE, FileCloser> file_;
  VpxFormat vpx_format_;
  uint32_t rate_;
  uint32_t scale_;
  uint32_t frame_count_;
};

}  // namespace VpxExample

#endif  // VPX_IOS_VPXEXAMPLE_IVF_FRAME_PARSER_H_

