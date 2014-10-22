// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef VPX_IOS_VPXEXAMPLE_VIDEO_BUFFER_POOL_H_
#define VPX_IOS_VPXEXAMPLE_VIDEO_BUFFER_POOL_H_

#include <CoreVideo/CoreVideo.h>

#include "./vpx_example_common.h"

namespace VpxExample {

// Creates and manages a buffer pool containing kNumVideoBuffers VideoBuffers.
class VideoBufferPool {
 public:
  struct VideoBuffer {
    VideoBuffer() : buffer(NULL), in_use(false), slot(0) {}
    CVPixelBufferRef buffer;
    bool in_use;
    size_t slot;
  };

  VideoBufferPool() : lock_(NULL) {}
  ~VideoBufferPool();

  // Allocates buffers stored in |buffer_pool_|. Returns true when successful.
  bool Init(size_t width, size_t height);

  // Finds an unused buffer in |buffer_pool_| and returns it. Returns NULL when
  // all buffers are in use.
  // This call blocks until |lock_| is obtained.
  const VideoBuffer *GetBuffer();

  // Releases |buffer| and returns it to the pool.
  // This call blocks until |lock_| is obtained.
  void ReleaseBuffer(const VideoBuffer *buffer);

 private:
  NSLock *lock_;
  VideoBuffer buffer_pool_[kNumVideoBuffers];
};

}  // namespace VpxExample

#endif  // VPX_IOS_VPXEXAMPLE_VIDEO_BUFFER_POOL_H_

