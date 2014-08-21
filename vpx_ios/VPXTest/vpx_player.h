// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef VPXTEST_VPXTEST_VPX_PLAYER_H_
#define VPXTEST_VPXTEST_VPX_PLAYER_H_

#include <memory>
#include <string>

#include "vpx_frame_parser.h"
#include "vpx_test_common.h"

struct vpx_codec_ctx;
struct vpx_image;

namespace VpxTest {

class VpxPlayer {
 public:
  VpxPlayer();
  ~VpxPlayer();

  bool PlayFile(const char *file_path);
  NSString *playback_result() const { return playback_result_; };

 private:
  bool InitParser();
  bool InitVpxDecoder();
  bool DeliverVideoBuffer(vpx_image *image);
  bool DecodeAllVideoFrames();

  NSString *playback_result_;
  std::unique_ptr<VpxFrameParserInterface> parser_;
  std::string file_path_;
  uint32_t frames_decoded_;
  VpxFormat format_;
  vpx_codec_ctx *vpx_codec_ctx_;
};

}  // namespace VpxTest

#endif  // VPXTEST_VPXTEST_VPX_PLAYER_H_
