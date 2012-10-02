// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef SHARED_WEBM_LIVE_MUXER_H_
#define SHARED_WEBM_LIVE_MUXER_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "webm_tools_types.h"

// Forward declarations of libwebm muxer types used by |WebMLiveMuxer|.
namespace mkvmuxer {
class Segment;
}

namespace webm_tools {

// Forward declaration of class implementing IMkvWriter interface for libwebm.
class WebMChunkWriter;

// WebM muxing class built atop libwebm. Provides buffers containing WebM
// "chunks". Chunks will be comprised of one or more full level 1 WebM
// Elements. The first chunk will include EBMLHeader, Segment, SegmentInfo,
// Tracks and may include one or more Cluster Elements.
//
// Notes:
// - Only the first chunk written is metadata. All other chunks are clusters.
//
// - All element size values are set to unknown (an EBML encoded -1).
//
// - Users MUST call |Init()| before any other method.
//
// - Users MUST call |Finalize()| to avoid losing the final cluster; libwebm
//   must buffer data in some situations to satisfy WebM container guidelines:
//   http://www.webmproject.org/code/specs/container/
//
// - Users are responsible for keeping memory usage reasonable by calling
//   |ChunkReady()| periodically-- when |ChunkReady| returns true,
//   |ReadChunk()| will return the complete chunk and discard it from the
//   buffer.
//
class WebMLiveMuxer {
 public:
  // Status codes returned by class methods.
  enum {
    // Temporary return code for unimplemented operations.
    kNotImplemented = -200,

    // Unable to write audio buffer.
    kAudioWriteError = -13,

    // |WriteAudioBuffer()| called without adding an audio track.
    kNoAudioTrack = -12,

    // Invalid |VorbisCodecPrivate| passed to |AddTrack()|.
    kAudioPrivateDataInvalid = -11,

    // |AddTrack()| called for audio, but the audio track has already been
    // added.
    kAudioTrackAlreadyExists = -10,

    // Addition of the audio track to |ptr_segment_| failed.
    kAudioTrackError = -9,

    // |ReadChunk()| called when no chunk is ready.
    kNoChunkReady = -8,

    // Buffer passed to |ReadChunk()| was too small.
    kUserBufferTooSmall = -7,

    // Unable to write video frame.
    kVideoWriteError = -6,

    // |WriteVideoFrame()| called without adding a video track.
    kNoVideoTrack = -5,

    // |AddTrack()| called for video, but the video track has already been
    // added.
    kVideoTrackAlreadyExists = -5,

    // Addition of the video track to |ptr_segment_| failed.
    kVideoTrackError = -4,

    // Something failed while interacting with the muxing library.
    kMuxerError = -3,

    kNoMemory = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };

  WebMLiveMuxer();
  ~WebMLiveMuxer();

  // Initializes libwebm for muxing in live mode, and adds tracks to
  // |ptr_segment_|. Passing a NULL configuration pointer disables the track of
  // that type. Returns |kSuccess| when successful.
  int Init();

  // Adds an audio track to |ptr_segment_| and returns the track number [1-127].
  // Returns |kAudioTrackAlreadyExists| when the audio track has already been
  // added. Returns |kAudioTrackError| when adding the track to the segment
  // fails.
  int AddAudioTrack(int sample_rate, int channels,
                    const uint8* private_data, size_t private_size);

  // Adds a video track to |ptr_segment_|, and returns the track number [1-127].
  // Returns |kVideoTrackAlreadyExists| when the video track has already been
  // added. Returns |kVideoTrackError| when adding the track to the segment
  // fails.
  int AddVideoTrack(int width, int height);

  // Sets the muxer's muxing app. Must be called before any frames are written.
  bool SetMuxingApp(const std::string& muxing_app);

  // Sets the muxer's writing app. Must be called before any frames are written.
  bool SetWritingApp(const std::string& writing_app);

  // Flushes any queued frames. Users MUST call this method to ensure that all
  // buffered frames are flushed out of libwebm. To determine if calling
  // |Finalize()| resulted in production of a chunk, call |ChunkReady()| after
  // the call to |Finalize()|. Returns |kSuccess| when |Segment::Finalize()|
  // returns without error.
  int Finalize();

  // Writes |data| to the audio Track. Returns kSuccess on success.
  int WriteAudioFrame(const uint8* data, size_t size,
                      uint64 timestamp_ns, bool is_key);

  // Writes |data| to the video Track. Returns kSuccess on success.
  int WriteVideoFrame(const uint8* data, size_t size,
                      uint64 timestamp_ns, bool is_key);

  // Writes |data| to the muxer. |size| is the size in bytes of |data|.
  // |timestamp_ns| is the timestamp of the frame in nanoseconds. |track_num|
  // is the Track number to add the frame. |is_key| flag telling if the frame
  // is a key frame. Returns kSuccess on success.
  int WriteFrame(const uint8* data, size_t size,
                 uint64 timestamp_ns, uint64 track_num, bool is_key);

  // Returns true and writes chunk length to |ptr_chunk_length| when |buffer_|
  // contains a complete WebM chunk.
  bool ChunkReady(int32* ptr_chunk_length);

  // Moves WebM chunk data into |ptr_buf|. The data has been from removed from
  // |buffer_| when |kSuccess| is returned.  Returns |kUserBufferTooSmall| if
  // |buffer_capacity| is less than |chunk_length|.
  int ReadChunk(int32 buffer_capacity, uint8* ptr_buf);

  // Accessors.
  bool initialized() const { return initialized_; }

 private:
  std::auto_ptr<WebMChunkWriter> ptr_writer_;
  std::auto_ptr<mkvmuxer::Segment> ptr_segment_;
  uint64 audio_track_num_;
  uint64 video_track_num_;
  std::vector<uint8> buffer_;
  bool initialized_;
  WEBM_TOOLS_DISALLOW_COPY_AND_ASSIGN(WebMLiveMuxer);
};

}  // namespace webm_tools

#endif  // SHARED_WEBM_LIVE_MUXER_H_
