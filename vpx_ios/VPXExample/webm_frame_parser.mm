// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "./webm_frame_parser.h"

#include <string>
#include <vector>

#include "WebM/mkvparser.hpp"
#include "WebM/mkvreader.hpp"

namespace VpxExample {

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
  NSUInteger video_track_num = 0;
  for (NSUInteger track_num = 1; track_num <= num_tracks; ++track_num) {
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

  video_track_num_ = video_track_num;
  NSLog(@"Found video track! \\o/ video_track_num=%llu video_codec_id=%s.",
        video_track_num_,
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

  const int64_t timecode_scale = segment_->GetInfo()->GetTimeCodeScale();
  const int64_t kExpectedWebmTimecodeScale = 1000000;
  if (timecode_scale != kExpectedWebmTimecodeScale) {
    // TODO(tomfinegan): Handle arbitrary timecode scales.
    NSLog(@"Unsupported timecode scale in WebM file (%lld).", timecode_scale);
    return false;
  }

  timebase_.numerator = timecode_scale;
  timebase_.denominator = kNanosecondsPerSecond;

  // Timecodes in WebM are milliseconds per the guidelines. Remove the common
  // factor from the timebase.
  // TODO(tomfinegan): Move timebase refactoring to a common location.
  if (timebase_.denominator > timebase_.numerator &&
      timebase_.denominator % timebase_.numerator == 0) {
    timebase_.denominator /= timebase_.numerator;
    timebase_.numerator = 1;
  }

  NSLog(@"WebM timebase %lld / %lld",
        timebase_.numerator, timebase_.denominator);

  return true;
}

bool WebmFrameParser::ReadFrame(VpxFrame *frame) {
  if (frame == NULL || frame->data == NULL)
    return false;

  bool got_frame = false;

  while (!got_frame) {
    if (!frame_head_.block_entry) {
      // Move to the next cluster.
      const mkvparser::Cluster *cluster = frame_head_.cluster;
      frame_head_.Reset();
      frame_head_.cluster = segment_->GetNext(cluster);

      if (frame_head_.cluster->EOS()) {
        NSLog(@"No more clusters.");
        return false;
      }

      if (frame_head_.cluster->GetFirst(frame_head_.block_entry)) {
        NSLog(@"No block in cluster.");
        return false;
      }

      frame_head_.block = frame_head_.block_entry->GetBlock();
    }

    // Skip blocks for non-video tracks.
    while (frame_head_.block->GetTrackNumber() != video_track_num_) {
      if (frame_head_.cluster->GetNext(frame_head_.block_entry,
                                       frame_head_.block_entry) < 0) {
        NSLog(@"Unable to parse next block.");
        return false;
      }

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

      if (frame->data->capacity() < mkvparser_frame.len) {
        frame->data->resize(mkvparser_frame.len * 2);
      }

      uint8_t *frame_data = &(*frame->data)[0];
      if (mkvparser_frame.Read(reader_.get(), frame_data)) {
        NSLog(@"Unable to read video frame");
        return false;
      }
      got_frame = true;
      frame->length = static_cast<uint32_t>(mkvparser_frame.len);
      frame->timebase = timebase_;
      frame->timestamp = frame_head_.block->GetTimeCode(frame_head_.cluster);

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

}  // namespace VpxExample
