// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef VPX_IOS_VPXEXAMPLE_WEBM_FRAME_PARSER_H_
#define VPX_IOS_VPXEXAMPLE_WEBM_FRAME_PARSER_H_

#include <memory>
#include <string>
#include <vector>

#include "./vpx_frame_parser.h"
#include "./vpx_test_common.h"

namespace mkvparser {
class Block;
class BlockEntry;
class Cluster;
class MkvReader;
class Segment;
}

namespace VpxExample {

// Tracks frame index when a block has multiple frames.
struct WebmBlockHead {
  WebmBlockHead() : frames_in_block(0), frame_index(0) {}
  void Reset() { frames_in_block = 0; frame_index = 0; }

  int32_t frames_in_block;
  int32_t frame_index;
};

// Tracks parser position. All pointer members unowned.
struct WebmFrameHead {
  WebmFrameHead() : block(NULL), block_entry(NULL), cluster(NULL) {}
  void Reset() {
    block = NULL;
    block_entry = NULL;
    cluster = NULL;
    block_head.Reset();
  }

  const mkvparser::Block *block;
  const mkvparser::BlockEntry *block_entry;
  const mkvparser::Cluster *cluster;

  WebmBlockHead block_head;
};

// Provides VPX frames from the first video track found in the file passed to
// HasVpxFrames().
class WebmFrameParser : public VpxFrameParserInterface {
 public:
  WebmFrameParser() : video_track_num_(0) {}
  virtual ~WebmFrameParser() {}
  bool HasVpxFrames(const std::string &file_path,
                    VpxFormat *vpx_format) override;
  bool ReadFrame(std::vector<uint8_t> *frame, uint32_t *frame_length) override;

 private:
  uint64_t video_track_num_;
  VpxFormat vpx_format_;
  WebmFrameHead frame_head_;

  std::unique_ptr<mkvparser::MkvReader> reader_;
  std::unique_ptr<mkvparser::Segment> segment_;
};

}  // namespace VpxExample

#endif  // VPX_IOS_VPXEXAMPLE_WEBM_FRAME_PARSER_H_

