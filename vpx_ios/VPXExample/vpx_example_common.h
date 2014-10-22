// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef VPX_IOS_VPXEXAMPLE_VPX_EXAMPLE_COMMON_H_
#define VPX_IOS_VPXEXAMPLE_VPX_EXAMPLE_COMMON_H_

// Comment the following line to enable usage of the basic webserver available
// in vpx_ios/testserver/server.py via python, i.e.:
// $ python path/to/webm-tools/vpx_ios/testserver/server.py
//
// Note: The above command must be run from a directory that contains IVF and/or
// WebM files, and those files must contain VP8 or VP9 video streams.
#define VPXEXAMPLE_LOCAL_PLAYBACK_ONLY

#ifdef VPXEXAMPLE_LOCAL_PLAYBACK_ONLY
#define kVp8File @"sintel_trailer_vp8.webm"
#define kVp9File @"sintel_trailer_vp9.webm"
#else
// These values must be adjusted when running on an iOS device. localhost is not
// the address you want on the device.
#define TEST_SERVER @"http://localhost:8000"

// Change allvpx to ivf to list only IVF files.
// Change allvpx to webm to list only WebM files.
#define TEST_FILE_LIST @"http://localhost:8000/allvpx"
#endif

namespace VpxExample {

const int kNumVideoBuffers = 8;
const int64_t kNanosecondsPerSecond = 1000000000;
const int64_t kMillisecondsPerSecond = 1000;

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

struct VpxTimeBase {
  VpxTimeBase() : numerator(1), denominator(1) {}
  int64_t numerator;
  int64_t denominator;
};

}  // namespace VpxExample

#endif  // VPX_IOS_VPXEXAMPLE_VPX_EXAMPLE_COMMON_H_

