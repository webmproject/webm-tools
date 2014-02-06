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
class ContentEncoding;
class SegmentInfo;
class Track;
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

  const uint64 audio_track_num =
      ptr_segment_->AddAudioTrack(sample_rate, channels, 0);
  if (!audio_track_num) {
    fprintf(stderr, "cannot AddAudioTrack on segment.\n");
    return kAudioTrackError;
  }
  mkvmuxer::AudioTrack* const ptr_audio_track =
      static_cast<mkvmuxer::AudioTrack*>(
          ptr_segment_->GetTrackByNumber(audio_track_num));
  if (!ptr_audio_track) {
    fprintf(stderr, "Unable to access audio track.\n");
    return kAudioTrackError;
  }
  if (!ptr_audio_track->SetCodecPrivate(private_data, private_size)) {
    fprintf(stderr, "Unable to write audio track codec private data.\n");
    return kAudioTrackError;
  }
  audio_track_num_ = audio_track_num;
  return audio_track_num_;
}

int WebMLiveMuxer::AddAudioTrack(int sample_rate, int channels,
                                 const uint8* private_data,
                                 size_t private_size,
                                 const std::string& codec_id) {
  if (codec_id.empty()) {
    fprintf(stderr, "Cannot AddAudioTrack with empty codec_id.\n");
    return kAudioTrackError;
  }
  const int status = AddAudioTrack(sample_rate, channels,
                                   private_data, private_size);
  if (status <= 0) {
    fprintf(stderr, "Cannot AddAudioTrack.\n");
    return status;
  }
  const int audio_track_num = status;
  mkvmuxer::AudioTrack* const audio_track =
      static_cast<mkvmuxer::AudioTrack*>(
          ptr_segment_->GetTrackByNumber(audio_track_num));
  if (!audio_track) {
    fprintf(stderr, "Unable to set audio codec_id: Track look up failed.\n");
    return kAudioTrackError;
  }
  audio_track->set_codec_id(codec_id.c_str());
  return audio_track_num;
}

bool WebMLiveMuxer::AddContentEncKeyId(uint64 track_num,
                                       const uint8* enc_key_id,
                                       size_t enc_key_id_size) {
  mkvmuxer::Track* const track = ptr_segment_->GetTrackByNumber(track_num);
  if (!track) {
    fprintf(stderr, "Could not get Track.\n");
    return false;
  }

  mkvmuxer::ContentEncoding* encoding = track->GetContentEncodingByIndex(0);
  if (!encoding) {
    if (!track->AddContentEncoding()) {
      fprintf(stderr, "Could not add ContentEncoding.\n");
      return false;
    }

    encoding = track->GetContentEncodingByIndex(0);
    if (!encoding) {
      fprintf(stderr, "Could not add ContentEncoding.\n");
      return false;
    }
  }

  if (!encoding->SetEncryptionID(enc_key_id, enc_key_id_size)) {
    fprintf(stderr, "Could not set encryption id for video track.\n");
    return false;
  }
  return true;
}

int WebMLiveMuxer::AddVideoTrack(int width, int height) {
  if (video_track_num_ != 0) {
    fprintf(stderr, "Cannot add video track: it already exists.\n");
    return kVideoTrackAlreadyExists;
  }
  const uint64 video_track_num = ptr_segment_->AddVideoTrack(width, height, 0);
  if (!video_track_num) {
    fprintf(stderr, "cannot AddVideoTrack on segment.\n");
    return kVideoTrackError;
  }
  video_track_num_ = video_track_num;
  return video_track_num_;
}


int WebMLiveMuxer::AddVideoTrack(int width, int height,
                                 const std::string& codec_id) {
  if (codec_id.empty()) {
    fprintf(stderr, "Cannot AddVideoTrack with empty codec_id.\n");
    return kVideoTrackError;
  }
  const int status = AddVideoTrack(width, height);
  if (status <= 0) {
    fprintf(stderr, "Cannot AddVideoTrack.\n");
    return status;
  }
  const int video_track_num = status;
  mkvmuxer::VideoTrack* const video_track =
      static_cast<mkvmuxer::VideoTrack*>(
          ptr_segment_->GetTrackByNumber(video_track_num));
  if (!video_track) {
    fprintf(stderr, "Unable to set video codec_id: Track look up failed.\n");
    return kVideoTrackError;
  }
  video_track->set_codec_id(codec_id.c_str());
  return video_track_num;
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
