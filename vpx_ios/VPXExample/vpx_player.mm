// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "./vpx_player.h"

#include <CoreVideo/CoreVideo.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <memory>
#include <vector>
#include <string>

#include "VPX/vpx/vpx_decoder.h"
#include "VPX/vpx/vp8dx.h"
#include "VPX/vpx/vpx_encoder.h"
#include "VPX/vpx/vpx_image.h"

#import "./ivf_frame_parser.h"
#include "mkvparser.hpp"
#include "mkvreader.hpp"
#import "./webm_frame_parser.h"

namespace {
// Expects YV12/I420 input. Creates NV12 |buffer|, and:
// - Copies Y-plane from |image| to |buffer|.
// - Interleaves the U and V planes of |image| into UV plane of |buffer|.
// Returns true upon success.
bool CopyVpxImageToPixelBuffer(const struct vpx_image *image,
                               CVPixelBufferRef buffer) {
  if (image == NULL || buffer == NULL) {
    NSLog(@"Invalid args in CopyVpxImageToPixelBuffer");
    return false;
  }

  // Lock the buffer (required for interaction with buffer).
  if (CVPixelBufferLockBaseAddress(buffer, 0) != kCVReturnSuccess) {
    NSLog(@"Unable to lock buffer");
    return false;
  }

  // Copy Y-plane.
  void *y_plane = CVPixelBufferGetBaseAddressOfPlane(buffer, 0);
  const size_t y_stride = CVPixelBufferGetBytesPerRowOfPlane(buffer, 0);

  for (int i = 0; i < image->d_h; ++i) {
    const uint8_t *src_line =
        image->planes[VPX_PLANE_Y] + i * image->stride[VPX_PLANE_Y];
    uint8_t *dst_line = reinterpret_cast<uint8_t *>(y_plane) + i * y_stride;
    memcpy(dst_line, src_line, image->d_w);
  }

  // Interleave U and V planes.
  uint8_t *uv_plane =
      reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(buffer, 1));
  const size_t uv_src_stride = image->stride[VPX_PLANE_U];
  const size_t uv_src_width = image->d_w >> image->y_chroma_shift;
  const size_t uv_dst_height = CVPixelBufferGetHeightOfPlane(buffer, 1);
  const size_t uv_dst_stride = CVPixelBufferGetBytesPerRowOfPlane(buffer, 1);

  // For each line of U and V...
  for (int line = 0; line < uv_dst_height; ++line) {
    const uint8_t *src_u_component =
        image->planes[VPX_PLANE_U] + uv_src_stride * line;
    const uint8_t *src_v_component =
        image->planes[VPX_PLANE_V] + uv_src_stride * line;
    uint8_t *dst_u_component = uv_plane + uv_dst_stride * line;
    uint8_t *dst_v_component = dst_u_component + 1;

    // For each pixel on the current line...
    for (int i = 0; i < uv_src_width; ++i) {
      // Copy U component.
      *dst_u_component = *src_u_component;
      ++src_u_component;
      dst_u_component += 2;
      // Copy V component.
      *dst_v_component = *src_v_component;
      ++src_v_component;
      dst_v_component += 2;
    }
  }

  // Unlock the buffer.
  if (CVPixelBufferUnlockBaseAddress(buffer, 0) != kCVReturnSuccess) {
    NSLog(@"Unable to unlock buffer.");
    return false;
  }

  return true;
}
}  // namespace

namespace VpxExample {

//
// VpxPlayer
//
VpxPlayer::VpxPlayer()
    : target_view_(NULL),
      playback_result_(nil),
      frames_decoded_(0),
      vpx_codec_ctx_(NULL),
      buffer_lock_(NULL) {
}

VpxPlayer::~VpxPlayer() {
  if (vpx_codec_ctx_ != NULL) {
    vpx_codec_destroy(vpx_codec_ctx_);
    delete vpx_codec_ctx_;
  }
}

void VpxPlayer::Init(GlkVideoViewController *target_view) {
  target_view_ = target_view;
  buffer_lock_ = [[NSLock alloc] init];
}

bool VpxPlayer::LoadFile(const char *file_path) {
  file_path_ = file_path;

  if (!InitParser()) {
    NSLog(@"VPx parser init failed.");
    return false;
  }

  if (!InitBufferPool()) {
    NSLog(@"Buffer pool init failed.");
    return false;
  }

  if (!InitVpxDecoder()) {
    NSLog(@"VPx decoder init failed.");
    return false;
  }

  return true;
}

bool VpxPlayer::Play() {
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

void VpxPlayer::ReleaseVideoBuffer(const VpxExample::VideoBuffer *buffer) {
  if (buffer != NULL)
    buffer_pool_.ReleaseBuffer(buffer);
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

bool VpxPlayer::InitBufferPool() {
  if (!buffer_pool_.Init(format_.width, format_.height)) {
    NSLog(@"Unable to initialize buffer pool.");
    return false;
  }
  return true;
}

bool VpxPlayer::InitVpxDecoder() {
  vpx_codec_dec_cfg_t vpx_config = {0};

  // Use number of physical CPUs threads for decoding.
  // TODO(tomfinegan): Add a check box or something to force single threaded
  // decode.
  unsigned int num_processors = 0;
  size_t length = sizeof(num_processors);
  sysctlbyname("hw.physicalcpu", &num_processors, &length, NULL, 0);
  vpx_config.threads = num_processors;

  vpx_codec_ctx_ = new vpx_codec_ctx();
  if (!vpx_codec_ctx_) {
    NSLog(@"Unable to allocate Vpx context.");
    return false;
  }

  NSLog(@"Decoding %dx%d %s", format_.width, format_.height,
        format_.codec == VP8 ? "VP8" : "VP9");

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

bool VpxPlayer::DeliverVideoBuffer(const vpx_image *image,
                                   const VideoBuffer *buffer) {
  if (target_view_ == NULL) {
    NSLog(@"No GlkVideoViewController.");
    return false;
  }

  if (!CopyVpxImageToPixelBuffer(image, buffer->buffer)) {
    NSLog(@"CopyVpxImageToPixelBuffer failed.");
    return false;
  }

  [target_view_ receiveVideoBuffer:reinterpret_cast<const void*>(buffer)];
  return true;
}

bool VpxPlayer::DecodeAllVideoFrames() {
  std::vector<uint8_t> vpx_frame;
  uint32_t frame_length = 0;
  // Time spent sleeping when no buffers are available in |buffer_pool_|.
  const float kSleepInterval = 1.0 / [target_view_ rendererFrameRate];

  while (parser_->ReadFrame(&vpx_frame, &frame_length)) {
    // Get a buffer for the output frame.
    const VideoBuffer *buffer = buffer_pool_.GetBuffer();

    // Wait until we actually have a buffer (if necessary).
    while (buffer == NULL) {
      [NSThread sleepForTimeInterval:kSleepInterval];
      buffer = buffer_pool_.GetBuffer();
    }

    const int codec_status =
        vpx_codec_decode(vpx_codec_ctx_,
                         &vpx_frame[0],
                         frame_length,
                         NULL, 0);
    if (codec_status == VPX_CODEC_OK)
      ++frames_decoded_;
    else
      NSLog(@"Decode failed after %d frames", frames_decoded_);

    vpx_codec_iter_t vpx_iter = NULL;
    vpx_image_t *vpx_image = vpx_codec_get_frame(vpx_codec_ctx_, &vpx_iter);

    if (vpx_image) {
      if (!DeliverVideoBuffer(vpx_image, buffer)) {
        NSLog(@"DeliverVideoBuffer failed.");
      }
    }
  }

  return true;
}

}  // namespace VpxExample
