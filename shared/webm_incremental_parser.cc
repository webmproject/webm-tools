/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webm_incremental_parser.h"

#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "mkvparser.hpp"
#include "mkvreader.hpp"
#include "webm_constants.h"
#include "webm_incremental_reader.h"

namespace mkvparser {
class BlockEntry;
class Tracks;
}  // namespace mkvparser

namespace webm_tools {

using std::string;
using std::vector;

WebMIncrementalParser::WebMIncrementalParser()
    : calculated_file_stats_(false),
      cluster_parse_offset_(0),
      parse_func_(&WebMIncrementalParser::ParseSegmentHeaders),
      ptr_cluster_(NULL),
      state_(kParsingHeader),
      total_bytes_parsed_(0) {
}

WebMIncrementalParser::~WebMIncrementalParser() {
}

bool WebMIncrementalParser::Init() {
  reader_.reset(new WebmIncrementalReader());
  if (!reader_.get()) {
    fprintf(stderr, "Could not create WebmIncrementalReader.\n");
    return false;
  }
  return true;
}

WebMIncrementalParser::Status WebMIncrementalParser::HasAudio(
    bool* value) const {
  assert(value);
  *value = false;

  if (state_ <= kParsingHeader)
    return state_;
  *value = (GetAudioTrack() != NULL);
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::AudioChannels(
    int* num_channels) const {
  assert(num_channels);
  *num_channels = 0;

  if (state_ <= kParsingHeader)
    return state_;
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  if (aud_track)
    *num_channels = static_cast<int>(aud_track->GetChannels());
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::AudioSampleRate(
    int* sample_rate) const {
  assert(sample_rate);
  *sample_rate = 0;

  if (state_ <= kParsingHeader)
    return state_;
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  if (aud_track)
    *sample_rate = static_cast<int>(aud_track->GetSamplingRate());
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::AudioSampleSize(
    int* sample_size) const {
  assert(sample_size);
  *sample_size = 0;

  if (state_ <= kParsingHeader)
    return state_;
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  if (aud_track)
    *sample_size = static_cast<int>(aud_track->GetBitDepth());
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::CalculateVideoFrameRate(
    double* framerate) const {
  assert(framerate);
  *framerate = 0.0;

  if (state_ <= kParsingHeader)
    return state_;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track)
    *framerate = CalculateFrameRate(vid_track->GetNumber());
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::CheckForCues(
    bool* value) const {
  assert(value);
  *value = false;

  if (state_ <= kParsingHeader)
    return state_;
  if (!segment_.get())
    return state_;

  const mkvparser::Cues* const cues = segment_->GetCues();
  if (!cues)
    return state_;

  // Load all the cue points
  while (!cues->DoneParsing()) {
    cues->LoadCuePoint();
  }

  // Get the first track. Shouldn't matter what track it is because the
  // tracks will be in separate files.
  const mkvparser::Track* const track = GetTrack(0);
  if (!track)
    return state_;

  const mkvparser::CuePoint* cue;
  const mkvparser::CuePoint::TrackPosition* track_position;

  // Check for the first cue.
  if (cues->Find(0, track, cue, track_position))
    *value = true;
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::CuesFirstInCluster(
    TrackTypes type, bool* value) const {
  assert(value);
  *value = false;

  if (state_ <= kParsingHeader)
    return state_;
  *value = CuesFirstInCluster(type);
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::FileAverageBitsPerSecond(
    int64* average_bps) const {
  assert(average_bps);
  *average_bps = 0;

  if (state_ <= kParsingHeader)
    return state_;
  int64 file_length;
  WebMIncrementalParser::Status status = FileLength(&file_length);
  if (status < state_)
    return status;

  int64 duration;
  status = GetDurationNanoseconds(&duration);
  if (status < state_ || duration == 0)
    return status;

  *average_bps = static_cast<int64>(8.0 * file_length /
                                    (duration / kNanosecondsPerSecond));
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::FileLength(
    int64* length) const {
  assert(length);
  *length = 0;

  if (state_ <= kParsingHeader)
    return state_;
  if (!reader_.get())
    return state_;

  int64 total;
  int64 available;
  if (reader_->Length(&total, &available) < 0)
    return state_;
  if (total > 0)
    *length = total;
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::FileMaximumBitsPerSecond(
    int64* maximum_bps) const {
  assert(maximum_bps);
  *maximum_bps = 0;

  if (state_ <= kParsingHeader)
    return state_;
  const mkvparser::Cues* const cues = GetCues();
  if (!cues)
    return state_;

  const mkvparser::CuePoint* cp = cues->GetFirst();
  int64 max_bps = 0;
  while (cp) {
    const int64 bps = CalculateBitsPerSecond(cp);
    if (bps > max_bps)
      max_bps = bps;
    cp = cues->GetNext(cp);
  }

  *maximum_bps = max_bps;
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::GetCodec(
    std::string* codec) const {
  assert(codec);
  codec->clear();

  if (state_ <= kParsingHeader)
    return state_;
  const string vorbis_id("A_VORBIS");
  const string vp8_id("V_VP8");
  string codec_temp;

  const mkvparser::Track* track = GetTrack(0);
  if (track) {
    const string codec_id(track->GetCodecId());
    if (codec_id == vorbis_id)
      codec_temp = "vorbis";
    else if (codec_id == vp8_id)
      codec_temp = "vp8";
  }

  track = GetTrack(1);
  if (track) {
    const string codec_id(track->GetCodecId());
    if (codec_id == vorbis_id) {
      if (!codec_temp.empty())
        codec_temp += ", ";
      codec_temp += "vorbis";
    } else if (codec_id == vp8_id) {
      if (!codec_temp.empty())
        codec_temp += ", ";
      codec_temp += "vp8";
    }
  }

  *codec = codec_temp;
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::GetCues(
    const mkvparser::Cues** cues) const {
  assert(cues);
  *cues = NULL;

  if (state_ <= kParsingHeader)
    return state_;
  *cues = GetCues();

  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::GetDurationNanoseconds(
    int64* duration) const {
  assert(duration);
  *duration = 0;

  if (state_ <= kParsingHeader)
    return state_;
  if (!segment_.get() || !segment_->GetInfo())
    return state_;
  *duration = segment_->GetInfo()->GetDuration();
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::GetMimeType(
    std::string* mimetype) const {
  assert(mimetype);
  mimetype->clear();

  if (state_ <= kParsingHeader)
    return state_;
  string mimetype_temp("video/webm");
  string codec;
  WebMIncrementalParser::Status status = GetCodec(&codec);
  if (status < state_)
    return status;

  if (codec == "vorbis")
    mimetype_temp = "audio/webm";

  *mimetype = mimetype_temp;
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::GetMimeTypeWithCodec(
    std::string* mimetype_with_codec) const {
  assert(mimetype_with_codec);
  mimetype_with_codec->clear();

  if (state_ <= kParsingHeader)
    return state_;
  string mimetype("video/webm");
  string codec;
  const string vorbis_id("A_VORBIS");
  const string vp8_id("V_VP8");
  const mkvparser::Track* track = GetTrack(0);
  if (track) {
    string codec_id(track->GetCodecId());
    if (codec_id == vorbis_id)
      codec = "vorbis";
    else if (codec_id == vp8_id)
      codec = "vp8";
  }

  track = GetTrack(1);
  if (track) {
    string codec_id(track->GetCodecId());
    if (codec_id == vorbis_id) {
      if (!codec.empty())
        codec += ", ";
      codec += "vorbis";
    } else if (codec_id == vp8_id) {
      if (!codec.empty())
        codec += ", ";
      codec += "vp8";
    }
  }

  if (!codec.empty()) {
    mimetype += "; codecs=\"" + codec + "\"";
  }

  *mimetype_with_codec = mimetype;
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::GetSegmentInfo(
    const mkvparser::SegmentInfo** info) const {
  assert(info);
  *info = NULL;

  if (state_ <= kParsingHeader)
    return state_;
  if (!segment_.get())
    return state_;
  *info = segment_->GetInfo();
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::GetSegmentStartOffset(
    int64* offset) const {
  assert(offset);
  *offset = -1;
  if (state_ <= kParsingHeader)
    return state_;
  if (!segment_.get())
    return state_;
  *offset = segment_->m_start;
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::OnlyOneStream(
    bool* value) const {
  assert(value);
  *value = false;

  if (state_ <= kParsingHeader)
    return state_;
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();

  if (!aud_track && !vid_track) {
    fprintf(stderr, "WebM file does not have an audio or video track.\n");
    return state_;
  }

  if (aud_track && vid_track)
    return state_;

  if (aud_track) {
    const string vorbis_id("A_VORBIS");
    const string codec_id(aud_track->GetCodecId());
    if (codec_id != vorbis_id) {
      fprintf(stderr, "Audio track does not match A_VORBIS. :%s\n",
              codec_id.c_str());
      return state_;
    }
  }

  if (vid_track) {
    const string vp8_id("V_VP8");
    const string codec_id(vid_track->GetCodecId());
    if (codec_id != vp8_id) {
      fprintf(stderr, "Video track does not match V_VP8. :%s\n",
              codec_id.c_str());
      return state_;
    }
  }

  *value = true;
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::ParseNextChunk(
    const Buffer& buf,
    int32* ptr_element_size) {
  if (!ptr_element_size) {
    fprintf(stderr, "NULL element size pointer!\n");
    return kParsingError;
  }
  if (buf.empty()) {
    return state_;
  }
  // Update |reader_|'s buffer window...
  if (reader_->SetBufferWindow(&buf[0], buf.size(), total_bytes_parsed_)) {
    fprintf(stderr, "could not update buffer window.\n");
    return kParsingError;
  }
  // Just return the result of the parsing attempt.
  return (this->*parse_func_)(ptr_element_size);
}

bool WebMIncrementalParser::SetEndOfFilePosition(int64 offset) {
  return reader_->SetEndOfSegmentPosition(offset);
}

WebMIncrementalParser::Status WebMIncrementalParser::TrackAverageBitsPerSecond(
    TrackTypes type, int64* average_bps) const {
  assert(average_bps);
  *average_bps = 0;

  if (state_ <= kParsingHeader)
    return state_;
  const mkvparser::Track* track = NULL;
  switch (type) {
    case kVideo:
      track = GetVideoTrack();
      break;
    case kAudio:
      track = GetAudioTrack();
      break;
    default:
      break;
  }
  if (!track)
    return state_;

  return CalculateTrackBitsPerSecond(track->GetNumber(), NULL, average_bps);
}

WebMIncrementalParser::Status WebMIncrementalParser::TrackCount(
    TrackTypes type, int64* count) const {
  assert(count);
  *count = 0;

  if (state_ <= kParsingHeader)
    return state_;
  if (!segment_.get())
    return state_;

  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  if (!tracks)
    return state_;

  int64 track_count = 0;
  uint32 i = 0;
  const uint32 j = tracks->GetTracksCount();

  while (i != j) {
    const mkvparser::Track* const track = tracks->GetTrackByIndex(i++);
    if (track == NULL)
      continue;

    if (track->GetType() == type)
      track_count++;
  }

  *count = track_count;
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::TrackFrameCount(
    TrackTypes type, int64* count) const {
  assert(count);
  *count = 0;

  if (state_ <= kParsingHeader)
    return state_;
  const mkvparser::Track* track = NULL;
  switch (type) {
    case kVideo:
      track = GetVideoTrack();
      break;
    case kAudio:
      track = GetAudioTrack();
      break;
    default:
      break;
  }
  if (!track)
    return state_;

  *count = CalculateTrackFrameCount(track->GetNumber(), NULL);
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::TrackSize(
    TrackTypes type, int64* size) const {
  assert(size);
  *size = 0;

  if (state_ <= kParsingHeader)
    return state_;
  const mkvparser::Track* track = NULL;
  switch (type) {
    case kVideo:
      track = GetVideoTrack();
      break;
    case kAudio:
      track = GetAudioTrack();
      break;
    default:
      break;
  }
  if (!track)
    return state_;

  *size = CalculateTrackSize(track->GetNumber(), NULL);
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::TrackStartNanoseconds(
    TrackTypes type, int64* start) const {
  assert(start);
  *start = 0;

  if (state_ <= kParsingHeader)
    return state_;
  if (!calculated_file_stats_)
    return state_;

  const mkvparser::Track* track = NULL;
  switch (type) {
    case kVideo:
      track = GetVideoTrack();
      break;
    case kAudio:
      track = GetAudioTrack();
      break;
    default:
      break;
  }
  if (!track)
    return state_;

  *start = tracks_start_milli_.find(track->GetNumber())->second;
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::HasVideo(
    bool* value) const {
  assert(value);
  *value = false;

  if (state_ <= kParsingHeader)
    return state_;
  *value = (GetVideoTrack() != NULL);
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::VideoHeight(
    int* height) const {
  assert(height);
  *height = 0;

  if (state_ <= kParsingHeader)
    return state_;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track)
    *height = static_cast<int>(vid_track->GetHeight());
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::VideoWidth(
    int* width) const {
  assert(width);
  *width = 0;

  if (state_ <= kParsingHeader)
    return state_;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track)
    *width = static_cast<int>(vid_track->GetWidth());
  return state_;
}

int64 WebMIncrementalParser::CalculateBitsPerSecond(
    const mkvparser::CuePoint* cp) const {
  if (!segment_.get())
    return 0;

  // Just estimate for now by parsing through some elements and getting the
  // highest byte value.
  const mkvparser::Cluster* cluster = NULL;
  if (cp)
    cluster = segment_->FindCluster(cp->GetTime(segment_.get()));
  else
    cluster = segment_->GetFirst();
  if (!cluster)
    return 0;

  int64 filesize = 0;
  const int64 start = cluster->GetTime();
  const int64 start_offset = cluster->m_element_start;
  while (cluster && !cluster->EOS()) {
    if ((cluster->m_element_start + cluster->GetElementSize()) > filesize)
      filesize = cluster->m_element_start + cluster->GetElementSize();

    cluster = segment_->GetNext(cluster);
  }

  if (!segment_->GetInfo())
    return 0;

  filesize -= start_offset;
  const int64 duration = segment_->GetInfo()->GetDuration() - start;
  if (duration <= 0)
    return 0;

  const double bitrate =
      (filesize * 8) / (duration / kNanosecondsPerSecond);
  return static_cast<int64>(bitrate);
}

double WebMIncrementalParser::CalculateFrameRate(int track_number) const {
  if (segment_->GetInfo()->GetDuration() == 0)
    return 0.0;
  if (!calculated_file_stats_)
    return 0;
  const int64 frames = tracks_frame_count_.find(track_number)->second;

  const double seconds =
      segment_->GetInfo()->GetDuration() / kNanosecondsPerSecond;
  const double frame_rate = frames / seconds;

  return frame_rate;
}

WebMIncrementalParser::Status WebMIncrementalParser::CalculateTrackBitsPerSecond(
    int track_number,
    const mkvparser::CuePoint* cp,
    int64* average_bps) const {
  *average_bps = 0;
  int64 size = 0;
  int64 start_ns = 0;
  if (cp == NULL) {
    if (!calculated_file_stats_)
      return state_;

    size = tracks_size_.find(track_number)->second;
  } else {
    const mkvparser::Cluster* cluster =
        segment_->FindCluster(cp->GetTime(segment_.get()));
    if (!cluster)
      return state_;

    start_ns = cluster->GetTime();
    while (cluster && !cluster->EOS()) {
      const mkvparser::BlockEntry* block_entry;
      int status = cluster->GetFirst(block_entry);
      if (status)
        return state_;

      while (block_entry && !block_entry->EOS()) {
        const mkvparser::Block* const block = block_entry->GetBlock();
        if (track_number == block->GetTrackNumber())
          size += block->m_size;

        status = cluster->GetNext(block_entry, block_entry);
        if (status)
          return state_;
      }

      cluster = segment_->GetNext(cluster);
    }
  }

  int64 duration;
  WebMIncrementalParser::Status status = GetDurationNanoseconds(&duration);
  if (status < state_ || duration == 0)
    return status;
  duration -= start_ns;
  if (duration == 0)
    return state_;

  *average_bps = static_cast<int64>(8.0 * size /
                                    (duration / kNanosecondsPerSecond));
  return state_;
}

int64 WebMIncrementalParser::CalculateTrackFrameCount(
    int track_number,
    const mkvparser::CuePoint* cp) const {
  int64 frames = 0;
  if (cp == NULL) {
    if (!calculated_file_stats_)
      return 0;

    frames = tracks_frame_count_.find(track_number)->second;
  } else {
    const mkvparser::Cluster* cluster =
        segment_->FindCluster(cp->GetTime(segment_.get()));
    if (!cluster)
      return 0;

    while (cluster && !cluster->EOS()) {
      const mkvparser::BlockEntry* block_entry;
      int status = cluster->GetFirst(block_entry);
      if (status)
        return false;

      while (block_entry && !block_entry->EOS()) {
        const mkvparser::Block* const block = block_entry->GetBlock();
        if (track_number == block->GetTrackNumber())
          frames++;

        status = cluster->GetNext(block_entry, block_entry);
        if (status)
          return 0;
      }

      cluster = segment_->GetNext(cluster);
    }
  }

  return frames;
}

int64 WebMIncrementalParser::CalculateTrackSize(
    int track_number,
    const mkvparser::CuePoint* cp) const {
  int64 size = 0;
  if (cp == NULL) {
    if (!calculated_file_stats_)
      return 0;

    size = tracks_size_.find(track_number)->second;
  } else {
    const mkvparser::Cluster* cluster =
        segment_->FindCluster(cp->GetTime(segment_.get()));
    if (!cluster)
      return 0;

    while (cluster && !cluster->EOS()) {
      const mkvparser::BlockEntry* block_entry;
      int status = cluster->GetFirst(block_entry);
      if (status)
        return false;

      while (block_entry && !block_entry->EOS()) {
        const mkvparser::Block* const block = block_entry->GetBlock();
        if (track_number == block->GetTrackNumber())
          size += block->m_size;

        status = cluster->GetNext(block_entry, block_entry);
        if (status)
          return 0;
      }

      cluster = segment_->GetNext(cluster);
    }
  }

  return size;
}

bool WebMIncrementalParser::CuesFirstInCluster(TrackTypes type) const {
  const mkvparser::Cues* const cues = GetCues();
  if (!cues)
    return false;

  const mkvparser::Track* track = NULL;
  switch (type) {
    case kVideo:
      track = GetVideoTrack();
      break;
    case kAudio:
      track = GetAudioTrack();
      break;
    default:
      track = GetTrack(0);
      break;
  }
  if (!track)
    return false;

  const mkvparser::CuePoint* cp = cues->GetFirst();
  if (!cp)
    return false;

  while (cp) {
    const mkvparser::Block* block = NULL;
    const mkvparser::Cluster* cluster = NULL;

    // Get the first block.
    if (!GetIndexedBlock(*cp, *track, 0, &cluster, &block))
      return false;

    if (!StartsWithKey(*cp, *cluster, *block))
      return false;
    cp = cues->GetNext(cp);
  }

  return true;
}

bool WebMIncrementalParser::GenerateStats() {
  if (state_ <= kParsingHeader)
    return false;

  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  if (!tracks)
    return 0;

  // TODO(fgalligan) Only calculate the stats from the last Cluster parsed.
  tracks_size_.clear();
  tracks_frame_count_.clear();
  tracks_start_milli_.clear();
  std::pair<std::map<int, int64>::iterator, bool> ret;
  const int32 track_count = static_cast<int32>(tracks->GetTracksCount());
  for (int i = 0; i < track_count; ++i) {
    const mkvparser::Track* const track = tracks->GetTrackByIndex(i);
    const int track_number = track->GetNumber();

    ret = tracks_size_.insert(std::pair<int, int64>(track_number, 0));
    if (!ret.second) {
      fprintf(stdout, "GenerateStats Could not insert tracks_size_.\n");
      return false;
    }
    ret = tracks_frame_count_.insert(std::pair<int, int64>(track_number, 0));
    if (!ret.second)
      return false;
    ret = tracks_start_milli_.insert(std::pair<int, int64>(track_number, -1));
    if (!ret.second)
      return false;
  }

  const mkvparser::Cluster* cluster = segment_->GetFirst();
  if (!cluster)
    return false;

  while (cluster && !cluster->EOS()) {
    const mkvparser::BlockEntry* block_entry;
    int status = cluster->GetFirst(block_entry);
    if (status) {
      fprintf(stdout, "GenerateStats Could not get first block_entry.\n");
      return false;
    }

    while (block_entry && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      const int track_number = block->GetTrackNumber();

      tracks_size_[track_number] += block->m_size;
      tracks_frame_count_[track_number]++;
      if (tracks_start_milli_[track_number] == -1)
        tracks_start_milli_[track_number] = block->GetTime(cluster);

      status = cluster->GetNext(block_entry, block_entry);
      if (status) {
        fprintf(stdout, "GenerateStats Could not get next block_entry.\n");
        return false;
      }
    }

    cluster = segment_->GetNext(cluster);
  }

  calculated_file_stats_ = true;
  return true;
}

const mkvparser::AudioTrack* WebMIncrementalParser::GetAudioTrack() const {
  if (!segment_.get())
    return NULL;

  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  if (!tracks)
    return NULL;

  uint32 i = 0;
  const uint32 j = tracks->GetTracksCount();
  while (i != j) {
    const mkvparser::Track* const track = tracks->GetTrackByIndex(i++);

    if (track == NULL)
      continue;

    if (track->GetType() == mkvparser::Track::kAudio)
      return static_cast<const mkvparser::AudioTrack*>(track);
  }

  return NULL;
}

const mkvparser::Cues* WebMIncrementalParser::GetCues() const {
  if (!segment_.get())
    return NULL;

  const mkvparser::Cues* const cues = segment_->GetCues();
  if (cues) {
    // Load all the cue points
    while (!cues->DoneParsing()) {
      cues->LoadCuePoint();
    }
  }

  return cues;
}

bool WebMIncrementalParser::GetIndexedBlock(
    const mkvparser::CuePoint& cp,
    const mkvparser::Track& track,
    int index,
    const mkvparser::Cluster** cluster,
    const mkvparser::Block** block) const {
  if (!cluster || !block)
    return false;

  const mkvparser::CuePoint::TrackPosition* const tp = cp.Find(&track);
  if (!tp)
    return false;

  // Find the first block that matches the CuePoint track number. This is
  // done because the block number may not be set which defaults to 1.
  const mkvparser::Cluster* const curr_cluster =
      segment_->FindCluster(cp.GetTime(segment_.get()));
  if (!curr_cluster)
    return false;

  const mkvparser::BlockEntry* block_entry = NULL;
  int status = curr_cluster->GetFirst(block_entry);
  if (status)
    return false;

  int block_index = -1;
  while (block_entry && !block_entry->EOS()) {
    const mkvparser::Block* const curr_block = block_entry->GetBlock();
    if (curr_block->GetTrackNumber() == tp->m_track) {
      block_index++;
      if (block_index == index) {
        *block = curr_block;
        *cluster = curr_cluster;
        return true;
      }
    }

    status = curr_cluster->GetNext(block_entry, block_entry);
    if (status)
      return false;
  }

  return false;
}

const mkvparser::Track* WebMIncrementalParser::GetTrack(uint32 index) const {
  if (!segment_.get())
    return NULL;

  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  if (!tracks)
    return NULL;

  return tracks->GetTrackByIndex(index);
}

const mkvparser::VideoTrack* WebMIncrementalParser::GetVideoTrack() const {
  if (!segment_.get())
    return NULL;

  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  if (!tracks)
    return NULL;

  uint32 i = 0;
  const uint32 j = tracks->GetTracksCount();

  while (i != j) {
    const mkvparser::Track* const track = tracks->GetTrackByIndex(i++);

    if (track == NULL)
      continue;

    if (track->GetType() == mkvparser::Track::kVideo)
      return static_cast<const mkvparser::VideoTrack*>(track);
  }

  return NULL;
}

WebMIncrementalParser::Status WebMIncrementalParser::ParseCluster(
    int32* ptr_element_size) {
  // A NULL |ptr_cluster_| means either:
  // - No clusters have been parsed, or...
  // - The last cluster was parsed.
  // In either case it's time to load a new one...
  int status;
  long length = 0;  // NOLINT
  if (!ptr_cluster_) {
    // Load/parse a cluster header...
    int64 current_pos = 0;
    status = segment_->LoadCluster(current_pos, length);
    if (status) {
      // TODO(fgalligan) Should we add a function to parse the Cues explictly?
      // Try to get the Cues to Load all of the cues.
      GetCues();

      fprintf(stdout, "LoadCluster returned 1. Parsed all Clusters.\n");
      state_ = kDoneParsing;
      return state_;
    }
    const mkvparser::Cluster* ptr_cluster = segment_->GetLast();
    assert(ptr_cluster);
    assert(!ptr_cluster->EOS());
    cluster_parse_offset_ = current_pos;
    ptr_cluster_ = ptr_cluster;
  }

  const int kClusterComplete = 1;
  for (;;) {
    status = ptr_cluster_->Parse(cluster_parse_offset_, length);
    if (status == kClusterComplete) {
      break;
    }
    if (status < 0) {
      return state_;
    }
  }

  int64 total;
  int64 avail;
  if (reader_->Length(&total, &avail) < 0)
    return kParsingError;

  const int64 cluster_size = ptr_cluster_->GetElementSize();
  const int64 cluster_pos_end = ptr_cluster_->m_element_start + cluster_size;
  if (cluster_pos_end > avail) {
    // The Cluster has been parsed but the reader does not have all of the
    // payload.
    fprintf(stdout,
            "Current cluster parsed, still need %lld bytes in the reader.\n",
            cluster_pos_end - avail);
    return state_;
  }

  if (cluster_size == -1) {
    // Should never happen... the parser should never report a complete cluster
    // without setting its length.
    return kParsingError;
  }

  if (!GenerateStats()) {
    fprintf(stderr, "GenerateStats returned ERROR.\n");
    return kParsingError;
  }

  ptr_cluster_ = NULL;
  cluster_parse_offset_ = 0;
  total_bytes_parsed_ += cluster_size;
  *ptr_element_size = static_cast<int32>(cluster_size);
  fprintf(stdout, "cluster_size=%lld total_bytes_parsed_=%lld\n",
          cluster_size, total_bytes_parsed_);
  return state_;
}

WebMIncrementalParser::Status WebMIncrementalParser::ParseSegmentHeaders(
    int32* ptr_element_size) {
  mkvparser::EBMLHeader ebml_header;
  int64 pos = 0;
  int64 parse_status = ebml_header.Parse(reader_.get(), pos);
  if (parse_status) {
    fprintf(stderr, "EBML header parse failed parse_status=%lld\n",
            parse_status);
    return state_;
  }

  // |pos| is equal to the length of the EBML header; start a running total now
  // since |ebml_header| doesn't store a length.
  int64 headers_length = pos;

  // Create and start parse of the segment...
  mkvparser::Segment* ptr_segment = NULL;
  parse_status = mkvparser::Segment::CreateInstance(reader_.get(), pos,
                                                    ptr_segment);
  if (parse_status) {
    fprintf(stderr, "Segment creation failed status=%lld\n", parse_status);
    return state_;
  }

  segment_.reset(ptr_segment);

  // Add the segment header length to the running total. The position argument
  // to the |CreateInstance| call above is not passed by reference (as is the
  // case with |ebml_header|), so |pos| is still correct.
  headers_length += segment_->m_start - pos;

  // |ParseHeaders| reads data until it runs out or finds a cluster. If it
  // finds a cluster, it returns 0 ONLY if segment info and segment tracks
  // elements were found as well.
  parse_status = segment_->ParseHeaders();
  if (parse_status) {
    fprintf(stderr, "Segment header parse failed parse_status=%lld\n",
            parse_status);
    return state_;
  }

  // Get the segment info to obtain its length.
  const mkvparser::SegmentInfo* ptr_segment_info = segment_->GetInfo();
  if (!ptr_segment_info) {
    fprintf(stderr, "Missing SegmentInfo.\n");
    return kParsingError;
  }

  // Get the segment tracks to obtain its length.
  const mkvparser::Tracks* ptr_tracks = segment_->GetTracks();
  if (!ptr_tracks) {
    fprintf(stderr, "Missing Tracks.\n");
    return kParsingError;
  }

  // TODO(fgalligan) We need a better way to figure out where the headers end.
  headers_length = ptr_tracks->m_element_start + ptr_tracks->m_element_size;

  total_bytes_parsed_ = headers_length;
  *ptr_element_size = static_cast<int32>(headers_length);
  parse_func_ = &WebMIncrementalParser::ParseCluster;
  state_ = kParsingClusters;
  return state_;
}

bool WebMIncrementalParser::StartsWithKey(const mkvparser::CuePoint& cp,
                                          const mkvparser::Cluster& cluster,
                                          const mkvparser::Block& block) const {
  // Check if the block is a key frame and check if the block has the
  // same time as the cue point.
  if (!block.IsKey())
    return false;

  if (block.GetTime(&cluster) != cp.GetTime(segment_.get()))
    return false;

  return true;
}

}  // namespace webm_tools
