// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webm_frame_parser.h"

#include <string>

#include "mkvparser.hpp"
#include "mkvreader.hpp"

namespace VpxTest {

bool WebmFrameParser::HasVpxFrames(const std::string &file_path,
                                   VpxFormat *vpx_format) {
  reader_.reset(new mkvparser::MkvReader);
  if (!reader_) {
    NSLog(@"MkvReader alloc failed.");
    return false;
  }

  if (reader_->Open(file_path.c_str()) != 0) {
    NSLog(@"Unable to open file: %s", file_path.c_str());
    return false;
  }

  std::unique_ptr<mkvparser::EBMLHeader> ebml_header;
  ebml_header.reset(new mkvparser::EBMLHeader);
  if (!ebml_header) {
    NSLog(@"EBMLHeader alloc failed.");
    return false;
  }

  int64_t pos = 0;
  if (ebml_header->Parse(reader_.get(), pos) != 0) {
    NSLog(@"EBMLHeader::Parse failed. Not a WebM file?");
    return false;
  }

  using mkvparser::Segment;
  Segment *raw_segment = NULL;
  if (Segment::CreateInstance(reader_.get(), pos, raw_segment) != 0) {
    NSLog(@"mkvparser::Segment::CreateInstance failed.");
    return false;
  }

  segment_.reset(raw_segment);
  if (segment_->Load() < 0) {
    NSLog(@"mkvparser::Segment::Load failed.");
    return false;
  }

  enum { kVideoTrackType = 1 };
  const mkvparser::Tracks *tracks_element = segment_->GetTracks();
  if (!tracks_element) {
    NSLog(@"mkvparser::Segment::GetTracks failed.");
    return false;
  }
  const uint64_t num_tracks = tracks_element->GetTracksCount();
  uint64_t video_track_num = 0;
  for (uint64_t track_num = 1; track_num <= num_tracks; ++track_num) {
    const mkvparser::Track *const track_element =
        tracks_element->GetTrackByNumber(track_num);
    if (track_element->GetType() == kVideoTrackType) {
      video_track_num = track_num;
      break;
    }
  }

  if (video_track_num == 0) {
    NSLog(@"Unable to find video track.");
    return false;
  }

  const mkvparser::VideoTrack *video_track =
      static_cast<const mkvparser::VideoTrack*>(
          tracks_element->GetTrackByNumber(video_track_num));
  const std::string video_codec_id = video_track->GetCodecId();
  VpxCodec codec = UNKNOWN;
  if (video_codec_id == "V_VP8") {
    codec = VP8;
  } else if (video_codec_id == "V_VP9") {
    codec = VP9;
  } else {
    NSLog(@"Unknown video codec ID (%s) in file %s.",
          video_codec_id.c_str(), file_path.c_str());
    return false;
  }

  vpx_format_.codec = codec;
  vpx_format_.width = static_cast<int>(video_track->GetWidth());
  vpx_format_.height = static_cast<int>(video_track->GetHeight());
  *vpx_format = vpx_format_;

  NSLog(@"Found video track! \\o/ video_track_num=%llu video_codec_id=%s.",
        video_track_num,
        video_codec_id.c_str());

  frame_head_.cluster = segment_->GetFirst();
  if (!frame_head_.cluster) {
    NSLog(@"No clusters in %s.", file_path.c_str());
    return false;
  }

  if (frame_head_.cluster->GetFirst(frame_head_.block_entry)) {
    NSLog(@"No block entry in first cluster of %s.", file_path.c_str());
    return false;
  }

  frame_head_.block = frame_head_.block_entry->GetBlock();
  video_track_num_ = video_track_num;
  return true;
}

bool WebmFrameParser::ReadFrame(std::vector<uint8_t> *frame,
                                uint32_t *frame_length) {
  if (!frame || !frame_length)
    return false;

  bool got_frame = false;

  while (!got_frame) {
    if (!frame_head_.block_entry) {
      // Move to the next cluster.
      //NSLog(@"Getting next cluster.");
      const mkvparser::Cluster *cluster = frame_head_.cluster;
      frame_head_.Reset();
      frame_head_.cluster = segment_->GetNext(cluster);

      if (frame_head_.cluster->EOS()) {
        NSLog(@"No more clusters.");
        return false;
      }
      //NSLog(@"Got cluster.");

      if (frame_head_.cluster->GetFirst(frame_head_.block_entry)) {
        NSLog(@"No block in cluster.");
        return false;
      }

      frame_head_.block = frame_head_.block_entry->GetBlock();
      //NSLog(@"Got first block entry in next cluster.");
    }

    // Skip blocks for non-video tracks.
    while (frame_head_.block->GetTrackNumber() != video_track_num_) {
      //NSLog(@"Skipping non-video block.");
      if (frame_head_.cluster->GetNext(frame_head_.block_entry,
                                       frame_head_.block_entry) < 0) {
        NSLog(@"Unable to parse next block.");
        return false;
      }
      //NSLog(@"Skipped block.");

      if (!frame_head_.block_entry || frame_head_.block_entry->EOS()) {
        frame_head_.block_entry = NULL;
        NSLog(@"Hit end of cluster while skipping non-video blocks.");
        break;
      }
      frame_head_.block = frame_head_.block_entry->GetBlock();
    }

    if (frame_head_.block_entry) {
      // Consume the block.
      const mkvparser::Block::Frame mkvparser_frame =
      frame_head_.block->GetFrame(frame_head_.block_head.frame_index);

      if (frame->capacity() < mkvparser_frame.len) {
        frame->resize(mkvparser_frame.len * 2);
      }

      uint8_t *frame_data = &(*frame)[0];
      if (mkvparser_frame.Read(reader_.get(), frame_data)) {
        NSLog(@"Unable to read video frame");
        return false;
      }
      got_frame = true;
      *frame_length = static_cast<uint32_t>(mkvparser_frame.len);
      //NSLog(@"got frame with length: %ld", mkvparser_frame.len);

      ++frame_head_.block_head.frame_index;
      frame_head_.block_head.frames_in_block =
          frame_head_.block->GetFrameCount();

      const WebmBlockHead &bh = frame_head_.block_head;
      if (bh.frame_index == bh.frames_in_block) {
        // Move to the next block.
        if (frame_head_.cluster->GetNext(frame_head_.block_entry,
                                         frame_head_.block_entry) < 0) {
          NSLog(@"Unable to parse next block.");
          return false;
        }

        if (!frame_head_.block_entry) {
          NSLog(@"Hit end of cluster getting next block.");
        } else {
          frame_head_.block = frame_head_.block_entry->GetBlock();
          frame_head_.block_head.Reset();
        }
      }
    }
  }

  return true;
}

}  // namespace VpxTest
