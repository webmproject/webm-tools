// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "vpx_player.h"

#include <memory>
#include <string>

#include "VPX/vpx/vpx_decoder.h"
#include "VPX/vpx/vp8dx.h"
#include "VPX/vpx/vp8cx.h"
#include "VPX/vpx/vpx_encoder.h"

#import "ivf_frame_parser.h"
#include "mkvparser.hpp"
#include "mkvreader.hpp"
#import "webm_frame_parser.h"

namespace VpxTest {

//
// VpxPlayer
//
VpxPlayer::VpxPlayer()
    : playback_result_(nil),
      frames_decoded_(0),
      vpx_codec_ctx_(NULL) {
}

VpxPlayer::~VpxPlayer() {
  if (vpx_codec_ctx_ != NULL) {
    vpx_codec_destroy(vpx_codec_ctx_);
    delete vpx_codec_ctx_;
  }
}

bool VpxPlayer::PlayFile(const char *file_path) {
  file_path_ = file_path;

  if (!InitParser()) {
    NSLog(@"WebM parser init failed.");
    return false;
  }

  if (!InitVpxDecoder()) {
    NSLog(@"VPX decoder init failed.");
    return false;
  }

  const double start_time = [[NSProcessInfo processInfo] systemUptime];
  const bool decode_result = DecodeAllVideoFrames();
  const double time_spent_decoding =
      [[NSProcessInfo processInfo] systemUptime] - start_time;

  playback_result_ =
      [NSString
          stringWithFormat:
              @"Decoded %d frames in %.2f seconds (%.2f frames/second). (%s)",
              frames_decoded_, time_spent_decoding,
              frames_decoded_ / time_spent_decoding,
              decode_result ? "No errors" : "Error"];
  NSLog(@"%@", playback_result_);

  return true;
}

bool VpxPlayer::InitParser() {
  parser_.reset(new IvfFrameParser());
  if (parser_->HasVpxFrames(file_path_, &format_)) {
    NSLog(@"Parsing %s as IVF", file_path_.c_str());
    return true;
  }

  parser_.reset(new WebmFrameParser());
  if (parser_->HasVpxFrames(file_path_, &format_)) {
    NSLog(@"Parsing %s as WebM", file_path_.c_str());
    return true;
  }

  NSLog(@"%s is not a supported file type, or it has no VPX frames to decode.",
        file_path_.c_str());
  return true;
}

bool VpxPlayer::InitVpxDecoder() {
  vpx_codec_dec_cfg_t vpx_config = {0};
  vpx_codec_ctx_ = new vpx_codec_ctx();
  if (!vpx_codec_ctx_) {
    NSLog(@"Unable to allocate Vpx context.");
    return false;
  }

  const int codec_status =
      vpx_codec_dec_init(vpx_codec_ctx_,
                         format_.codec == VP8 ?
                             vpx_codec_vp8_dx() : vpx_codec_vp9_dx(),
                         &vpx_config,
                         0);
  if (codec_status) {
    NSLog(@"Libvpx decoder init failed: %d", codec_status);
    return false;
  }
  return true;
}

bool VpxPlayer::DecodeAllVideoFrames() {
  std::vector<uint8_t> vpx_frame;
  uint32_t frame_length = 0;

  while (parser_->ReadFrame(&vpx_frame, &frame_length)) {
    //NSLog(@"Decoding frame with length: %u", frame_length);
    const int codec_status =
        vpx_codec_decode(vpx_codec_ctx_,
                         &vpx_frame[0],
                         frame_length,
                         NULL, 0);
    if (codec_status == VPX_CODEC_OK)
      ++frames_decoded_;
    else
      NSLog(@"Decode failed after %d frames", frames_decoded_);
  }

  return true;
}

}  // namespace VpxTest
