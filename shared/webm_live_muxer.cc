// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webm_live_muxer.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "mkvmuxer.hpp"
#include "webmids.hpp"
#include "webm_chunk_writer.h"

namespace mkvmuxer {
class AudioTrack;
class SegmentInfo;
}

namespace webm_tools {

///////////////////////////////////////////////////////////////////////////////
// WebMLiveMuxer
//

WebMLiveMuxer::WebMLiveMuxer()
    : audio_track_num_(0),
      video_track_num_(0),
      initialized_(false) {
}

WebMLiveMuxer::~WebMLiveMuxer() {
}

int WebMLiveMuxer::Init() {
  // Construct and Init |WebMChunkWriter|. It handles writes coming from
  // libwebm.
  ptr_writer_.reset(new (std::nothrow) WebMChunkWriter());  // NOLINT
  if (!ptr_writer_.get()) {
    fprintf(stderr, "Cannot construct WebmWriteBuffer.\n");
    return kNoMemory;
  }
  if (ptr_writer_->Init(&buffer_)) {
    fprintf(stderr, "Cannot Init WebmWriteBuffer.\n");
    return kMuxerError;
  }

  // Construct and Init |ptr_segment_|, then enable live mode.
  ptr_segment_.reset(new (std::nothrow) mkvmuxer::Segment());  // NOLINT
  if (!ptr_segment_.get()) {
    fprintf(stderr, "Cannot construct Segment.\n");
    return kNoMemory;
  }

  if (!ptr_segment_->Init(ptr_writer_.get())) {
    fprintf(stderr, "Cannot Init Segment.\n");
    return kMuxerError;
  }

  ptr_segment_->set_mode(mkvmuxer::Segment::kLive);

  // Set segment info fields.
  using mkvmuxer::SegmentInfo;
  SegmentInfo* const ptr_segment_info = ptr_segment_->GetSegmentInfo();
  if (!ptr_segment_info) {
    fprintf(stderr, "Segment has no SegmentInfo.\n");
    return kNoMemory;
  }

  // Set writing application name.
  ptr_segment_info->set_writing_app("WebMLiveMuxer");
  initialized_ = true;
  return kSuccess;
}



int WebMLiveMuxer::AddAudioTrack(int sample_rate, int channels,
                                 const uint8* private_data,
                                 size_t private_size) {
  if (audio_track_num_ != 0) {
    fprintf(stderr, "Cannot add audio track: it already exists.\n");
    return kAudioTrackAlreadyExists;
  }

  audio_track_num_ = ptr_segment_->AddAudioTrack(sample_rate, channels, 0);
  if (!audio_track_num_) {
    fprintf(stderr, "cannot AddAudioTrack on segment.\n");
    return kAudioTrackError;
  }
  mkvmuxer::AudioTrack* const ptr_audio_track =
      static_cast<mkvmuxer::AudioTrack*>(
          ptr_segment_->GetTrackByNumber(audio_track_num_));
  if (!ptr_audio_track) {
    fprintf(stderr, "Unable to access audio track.\n");
    return kAudioTrackError;
  }
  if (!ptr_audio_track->SetCodecPrivate(private_data, private_size)) {
    fprintf(stderr, "Unable to write audio track codec private data.\n");
    return kAudioTrackError;
  }
  return audio_track_num_;
}

int WebMLiveMuxer::AddVideoTrack(int width, int height) {
  if (video_track_num_ != 0) {
    fprintf(stderr, "Cannot add video track: it already exists.\n");
    return kVideoTrackAlreadyExists;
  }
  video_track_num_ = ptr_segment_->AddVideoTrack(width, height, 0);
  if (!video_track_num_) {
    fprintf(stderr, "cannot AddVideoTrack on segment.\n");
    return kVideoTrackError;
  }
  return video_track_num_;
}

bool WebMLiveMuxer::SetMuxingApp(const std::string& muxing_app) {
  using mkvmuxer::SegmentInfo;
  SegmentInfo* const ptr_segment_info = ptr_segment_->GetSegmentInfo();
  if (!ptr_segment_info) {
    fprintf(stderr, "Could not set muxing app.\n");
    return false;
  }

  ptr_segment_info->set_muxing_app(muxing_app.c_str());
  return true;
}

bool WebMLiveMuxer::SetWritingApp(const std::string& writing_app) {
  using mkvmuxer::SegmentInfo;
  SegmentInfo* const ptr_segment_info = ptr_segment_->GetSegmentInfo();
  if (!ptr_segment_info) {
    fprintf(stderr, "Could not set writing app.\n");
    return false;
  }

  ptr_segment_info->set_writing_app(writing_app.c_str());
  return true;
}

int WebMLiveMuxer::Finalize() {
  if (!ptr_segment_->Finalize()) {
    fprintf(stderr, "libwebm mkvmuxer Finalize failed.\n");
    return kMuxerError;
  }

  if (buffer_.size() > 0) {
    // When data is in |buffer_| after the |mkvmuxer::Segment::Finalize()|
    // call, make the last chunk available to the user by forcing
    // |ChunkReady()| to return true one final time. This last chunk will
    // contain any data passed to |mkvmuxer::Segment::AddFrame()| since the
    // last call to |WebMChunkWriter::ElementStartNotify()|.
    ptr_writer_->ElementStartNotify(mkvmuxer::kMkvCluster,
                                    ptr_writer_->bytes_written());
  }

  return kSuccess;
}

int WebMLiveMuxer::WriteAudioFrame(const uint8* data, size_t size,
                                   uint64 timestamp_ns, bool is_key) {
  if (audio_track_num_ == 0) {
    fprintf(stderr, "Cannot WriteAudioFrame without an audio track.\n");
    return kNoAudioTrack;
  }
  return WriteFrame(data, size, timestamp_ns, audio_track_num_, is_key);
}

int WebMLiveMuxer::WriteVideoFrame(const uint8* data, size_t size,
                                   uint64 timestamp_ns, bool is_key ) {
  if (video_track_num_ == 0) {
    fprintf(stderr, "Cannot WriteVideoFrame without a video track.\n");
    return kNoVideoTrack;
  }
  return WriteFrame(data, size, timestamp_ns, video_track_num_, is_key);
}

int WebMLiveMuxer::WriteFrame(const uint8* data, size_t size,
                              uint64 timestamp_ns, uint64 track_num,
                              bool is_key) {
  if (!ptr_segment_->AddFrame(data,
                              size,
                              track_num,
                              timestamp_ns,
                              is_key)) {
    fprintf(stderr, "AddFrame failed.\n");
    return kVideoWriteError;
  }
  return kSuccess;
}

// A chunk is ready when |WebMChunkWriter::chunk_length()| returns a value
// greater than 0.
bool WebMLiveMuxer::ChunkReady(int32* ptr_chunk_length) {
  if (ptr_chunk_length) {
    const int32 chunk_length = static_cast<int32>(ptr_writer_->chunk_end());
    if (chunk_length > 0) {
      *ptr_chunk_length = chunk_length;
      return true;
    }
  }
  return false;
}

// Copies the buffered chunk data into |ptr_buf|, erases it from |buffer_|, and
// calls |WebMChunkWriter::ResetChunkEnd()| to zero the chunk end position.
int WebMLiveMuxer::ReadChunk(int32 buffer_capacity, uint8* ptr_buf) {
  if (!ptr_buf) {
    fprintf(stderr, "NULL buffer pointer.\n");
    return kInvalidArg;
  }

  // Make sure there's a chunk ready.
  int32 chunk_length = 0;
  if (!ChunkReady(&chunk_length)) {
    fprintf(stderr, "No chunk ready.\n");
    return kNoChunkReady;
  }

  // Confirm user buffer is of adequate size
  if (buffer_capacity < chunk_length) {
    fprintf(stderr, "Not enough space for chunk.\n");
    return kUserBufferTooSmall;
  }

  fprintf(stdout, "ReadChunk capacity=%d length=%d total buffered=%zd\n",
          buffer_capacity, chunk_length, buffer_.size());

  // Copy chunk to user buffer, and erase it from |buffer_|.
  memcpy(ptr_buf, &buffer_[0], chunk_length);
  ptr_writer_->EraseChunk();
  return kSuccess;
}

}  // namespace webm_tools
