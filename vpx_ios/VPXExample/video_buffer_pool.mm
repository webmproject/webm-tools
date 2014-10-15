// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "./video_buffer_pool.h"

namespace VpxExample {

class AutoLock {
 public:
  explicit AutoLock(NSLock *lock) : lock_(lock) { [lock_ lock]; }
  ~AutoLock() { [lock_ unlock]; }

 private:
  NSLock *lock_;
};

VideoBufferPool::~VideoBufferPool() {
  AutoLock lock(lock_);
  for (int i = 0; i < kNumVideoBuffers; ++i) {
    if (buffer_pool_[i].buffer != NULL) {
      CVPixelBufferRelease(buffer_pool_[i].buffer);
    }
  }
}

bool VideoBufferPool::Init(size_t width, size_t height) {
  lock_ = [[NSLock alloc] init];

  // Note: kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange is NV12.
  const OSType pixel_fmt = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;

  const NSDictionary *pixelBufferAttributes =
      [NSDictionary dictionaryWithObjectsAndKeys:[NSDictionary dictionary],
          kCVPixelBufferIOSurfacePropertiesKey, nil];
  const CFDictionaryRef cfPixelBufferAttributes =
      (__bridge CFDictionaryRef)pixelBufferAttributes;

  for (int i = 0; i < kNumVideoBuffers; ++i) {
    CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault,
                                          width, height,
                                          pixel_fmt,
                                          cfPixelBufferAttributes,
                                          &buffer_pool_[i].buffer);
    if (status != kCVReturnSuccess) {
      NSLog(@"CVPixelBufferRef creation failed %d", status);
      return false;
    }
    buffer_pool_[i].slot = i;
  }
  return true;
}

const VideoBufferPool::VideoBuffer *VideoBufferPool::GetBuffer() {
  AutoLock lock(lock_);
  const VideoBuffer *buffer = NULL;
  for (int i = 0; i < kNumVideoBuffers; ++i) {
    if (!buffer_pool_[i].in_use) {
      buffer_pool_[i].in_use = true;
      buffer = &buffer_pool_[i];
      break;
    }
  }
  return buffer;
}

void VideoBufferPool::ReleaseBuffer(
    const VideoBufferPool::VideoBuffer *buffer) {
  AutoLock lock(lock_);
  buffer_pool_[buffer->slot].in_use = false;
}

}  // namespace VpxExample
