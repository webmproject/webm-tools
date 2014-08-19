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

#include <CoreVideo/CoreVideo.h>

#include "VPX/vpx/vpx_decoder.h"
#include "VPX/vpx/vp8dx.h"
#include "VPX/vpx/vpx_encoder.h"
#include "VPX/vpx/vpx_image.h"

#import "ivf_frame_parser.h"
#include "mkvparser.hpp"
#include "mkvreader.hpp"
#import "webm_frame_parser.h"

namespace {
bool CopyVpxImageToPixelBuffer(const struct vpx_image *image,
                               CVPixelBufferRef buffer) {
  if (!image || !buffer) {
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

  const uint8_t *src_line;
  uint8_t *dst_line;
  for (int i = 0; i < image->d_h; ++i) {
    src_line = image->planes[VPX_PLANE_Y] + i * image->stride[VPX_PLANE_Y];
    dst_line = reinterpret_cast<uint8_t *>(y_plane) + i * y_stride;
    memcpy(dst_line, src_line, image->d_w);
  }

  // Interleave U and V planes.
  uint8_t *uv_plane =
      reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(buffer, 1));
  const size_t uv_stride = CVPixelBufferGetBytesPerRowOfPlane(buffer, 1);
  const size_t uv_height = CVPixelBufferGetHeightOfPlane(buffer, 1);
  const size_t src_width = image->stride[VPX_PLANE_U];

  // For each line of U and V...
  for (int line = 0; line < uv_height; ++line) {
    uint8_t *src_u_component = image->planes[VPX_PLANE_U] + src_width * line;
    uint8_t *dst_u_component = uv_plane + uv_stride * line;
    uint8_t *src_v_component = image->planes[VPX_PLANE_V] + src_width * line;
    uint8_t *dst_v_component = dst_u_component + 1;

    // For each pixel on the current line...
    for (int i = 0; i < src_width; ++i) {
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

bool VpxPlayer::DeliverVideoBuffer(struct vpx_image *image) {
  if (!target_view_) {
    NSLog(@"No GlkVideoViewController.");
    return false;
  }
  // TODO(tomfinegan): This is horribly inefficient. Preallocate a pool of these
  // (CVPixelBufferPool?) instead of leaking them to
  // CVPixelBufferReceiverInterface and relying on the receiver to clean up
  // after VpxPlayer.
  CVPixelBufferRef buffer;

  // Note: kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange is NV12.
  const OSType pixel_fmt = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;

  NSDictionary *pixelBufferAttributes =
      [NSDictionary dictionaryWithObjectsAndKeys:[NSDictionary dictionary],
          kCVPixelBufferIOSurfacePropertiesKey, nil];
  CFDictionaryRef cfPixelBufferAttributes =
      (__bridge CFDictionaryRef)pixelBufferAttributes;

  CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault,
                                        image->d_w, image->d_h,
                                        pixel_fmt,
                                        cfPixelBufferAttributes,
                                        &buffer);
  if (status != kCVReturnSuccess) {
    NSLog(@"CVPixelBufferRef creation failed %d", status);
    return false;
  }

  if (!CopyVpxImageToPixelBuffer(image, buffer)) {
    NSLog(@"CopyVpxImageToPixelBuffer failed.");
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

    vpx_codec_iter_t vpx_iter = NULL;
    vpx_image_t *vpx_image = vpx_codec_get_frame(vpx_codec_ctx_, &vpx_iter);

    if (vpx_image) {
      if (!DeliverVideoBuffer(vpx_image)) {
        NSLog(@"DeliverVideoBuffer failed.");
      }
    }
  }

  return true;
}

}  // namespace VpxTest
