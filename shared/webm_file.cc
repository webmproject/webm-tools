/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webm_file.h"

#include <stddef.h>
#include <stdio.h>
#include <climits>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "mkvparser.hpp"
#include "mkvreader.hpp"
#include "webm_consts.h"

namespace mkvparser {
class BlockEntry;
class Tracks;
}  // namespace mkvparser

namespace webm_tools {

using std::string;
using std::vector;

typedef vector<const WebMFile*>::const_iterator WebMConstIterator;
typedef vector<const mkvparser::Cues*>::const_iterator CuesConstIterator;

WebMFile::WebMFile(const string& filename)
    : calculated_file_stats_(false),
      cue_chunk_time_nano_(LLONG_MAX),
      filename_(filename) {
}

WebMFile::~WebMFile() {
}

bool WebMFile::Init() {
  reader_.reset(new mkvparser::MkvReader());

  if (reader_->Open(filename_.c_str())) {
    fprintf(stderr, "Error trying to open file:%s\n", filename_.c_str());
    return false;
  }

  int64 pos = 0;
  mkvparser::EBMLHeader ebml_header;
  if (ebml_header.Parse(reader_.get(), pos) < 0) {
    fprintf(stderr, "EBMLHeader Parse() failed.\n");
    return false;
  }

  if (!CheckDocType(ebml_header.m_docType)) {
    fprintf(stderr, "DocType != webm\n");
    return false;
  }

  mkvparser::Segment* segment;
  if (mkvparser::Segment::CreateInstance(reader_.get(), pos, segment)) {
    fprintf(stderr, "Segment::CreateInstance() failed.\n");
    return false;
  }

  segment_.reset(segment);
  if (segment_->Load() < 0) {
    fprintf(stderr, "Segment::Load() failed.\n");
    return false;
  }

  if (!LoadCueDescList()) {
    fprintf(stderr, "LoadCueDescList() failed.\n");
    return false;
  }

  if (!GenerateStats()) {
    fprintf(stderr, "GenerateStats() failed.\n");
    return false;
  }

  return true;
}

bool WebMFile::HasAudio() const {
  return (GetAudioTrack() != NULL);
}

int WebMFile::AudioChannels() const {
  int channels = 0;
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  if (aud_track) {
    channels = static_cast<int>(aud_track->GetChannels());
  }

  return channels;
}

int WebMFile::AudioSampleRate() const {
  int sample_rate = 0;
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  if (aud_track) {
    sample_rate = static_cast<int>(aud_track->GetSamplingRate());
  }

  return sample_rate;
}

int WebMFile::AudioSampleSize() const {
  int sample_size = 0;
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  if (aud_track) {
    sample_size = static_cast<int>(aud_track->GetBitDepth());
  }

  return sample_size;
}

int WebMFile::BufferSizeAfterTime(double time,
                                  double search_sec,
                                  int64 kbps,
                                  double* buffer,
                                  double* sec_counted) const {
  if (!buffer)
    return -1;

  const int64 time_ns = static_cast<int64>(time * kNanosecondsPerSecond);

  const CueDesc* descCurr = GetCueDescFromTime(time_ns);
  if (!descCurr)
    return -1;

  // TODO(fgalligan): Handle non-cue start time.
  if (descCurr->start_time_ns != time_ns)
    return -1;

  double sec_downloading = 0.0;
  double sec_downloaded = 0.0;

  do {
    const int64 desc_bytes = descCurr->end_offset - descCurr->start_offset;
    const double desc_sec =
        (descCurr->end_time_ns - descCurr->start_time_ns) /
        kNanosecondsPerSecond;
    const double time_to_download = ((desc_bytes * 8) / 1000.0) / kbps;

    sec_downloading += time_to_download;
    sec_downloaded += desc_sec;

    if (sec_downloading > search_sec) {
      sec_downloaded = (search_sec / sec_downloading) * sec_downloaded;
      sec_downloading = search_sec;
      break;
    }

    descCurr = GetCueDescFromTime(descCurr->end_time_ns);
  } while (descCurr);

  *buffer = sec_downloaded - sec_downloading + *buffer;
  if (sec_counted)
    *sec_counted = sec_downloading;

  return 0;
}

int WebMFile::BufferSizeAfterTimeDownloaded(int64 time_ns,
                                            double search_sec,
                                            int64 bps,
                                            double min_buffer,
                                            double* buffer,
                                            double* sec_to_download) const {
  if (!buffer || !sec_to_download)
    return -1;

  const double time_sec = time_ns / kNanosecondsPerSecond;

  const CueDesc* descCurr = GetCueDescFromTime(time_ns);
  if (!descCurr)
    return -1;

  int rv = 0;
  const int64 time_to_search_ns =
      static_cast<int64>(search_sec * kNanosecondsPerSecond);
  const int64 end_time_ns = time_ns + time_to_search_ns;
  *sec_to_download = 0.0;
  double sec_downloaded = 0.0;

  // Check for non cue start time.
  if (time_ns > descCurr->start_time_ns) {
    const int64 cue_nano = descCurr->end_time_ns - time_ns;
    const double percent =
        static_cast<double>(cue_nano) /
        (descCurr->end_time_ns - descCurr->start_time_ns);
    const double cueBytes =
        (descCurr->end_offset - descCurr->start_offset) * percent;
    const double timeToDownload = (cueBytes * 8.0) / bps;

    sec_downloaded += (cue_nano / kNanosecondsPerSecond) - timeToDownload;
    *sec_to_download += timeToDownload;

    // Check if the search ends within the first cue.
    if (descCurr->end_time_ns >= end_time_ns) {
      const double desc_end_time_sec =
          descCurr->end_time_ns / kNanosecondsPerSecond;
      const double percent_to_sub =
          search_sec / (desc_end_time_sec - time_sec);
      sec_downloaded = percent_to_sub * sec_downloaded;
      *sec_to_download = percent_to_sub * *sec_to_download;
    }

    if ((sec_downloaded + *buffer) <= min_buffer) {
      return 1;
    }

    // Get the next Cue.
    descCurr = GetCueDescFromTime(descCurr->end_time_ns);
  }

  while (descCurr) {
    const int64 desc_bytes = descCurr->end_offset - descCurr->start_offset;
    const int64 desc_ns = descCurr->end_time_ns - descCurr->start_time_ns;
    const double desc_sec = desc_ns / kNanosecondsPerSecond;
    const double bits = (desc_bytes * 8.0);
    const double time_to_download = bits / bps;

    sec_downloaded += desc_sec - time_to_download;
    *sec_to_download += time_to_download;

    if (descCurr->end_time_ns >= end_time_ns) {
      const double desc_end_time_sec =
          descCurr->end_time_ns / kNanosecondsPerSecond;
      const double percent_to_sub =
          search_sec / (desc_end_time_sec - time_sec);
      sec_downloaded = percent_to_sub * sec_downloaded;
      *sec_to_download = percent_to_sub * *sec_to_download;

      if ((sec_downloaded + *buffer) <= min_buffer)
        rv = 1;
      break;
    }

    if ((sec_downloaded + *buffer) <= min_buffer) {
      rv = 1;
      break;
    }

    descCurr = GetCueDescFromTime(descCurr->end_time_ns);
  }

  *buffer = *buffer + sec_downloaded;

  return rv;
}

double WebMFile::CalculateVideoFrameRate() const {
  double rate = 0.0;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track) {
    rate = CalculateFrameRate(vid_track->GetNumber());
  }

  return rate;
}

bool WebMFile::CheckBitstreamSwitching(const WebMFile& webm_file) const {
  const mkvparser::Track* const track = webm_file.GetTrack(0);
  const mkvparser::Track* const track_int = GetTrack(0);
  if (!track || !track_int)
    return false;

  if (track->GetNumber() != track_int->GetNumber())
    return false;

  const string codec = track->GetCodecId();
  const string codec_int = track_int->GetCodecId();
  if (codec != codec_int)
    return false;

  size_t size = 0;
  size_t size_int = 0;
  const uint8* codec_priv = track->GetCodecPrivate(size);
  const uint8* codec_priv_int = track_int->GetCodecPrivate(size_int);
  if (size != size_int)
    return false;

  if (memcmp(codec_priv, codec_priv_int, size))
    return false;

  return true;
}

bool WebMFile::CheckCuesAlignment(const WebMFile& webm_file) const {
  const mkvparser::Cues* const cues = webm_file.GetCues();
  if (!cues)
    return false;

  const mkvparser::Cues* const cues_int = GetCues();
  if (!cues_int)
    return false;

  const mkvparser::Track* const track = webm_file.GetTrack(0);
  const mkvparser::Track* const track_int = GetTrack(0);
  if (!track || !track_int)
    return false;

  if (cues->GetCount() != cues_int->GetCount())
    return false;

  const mkvparser::CuePoint* cp = cues->GetFirst();
  const mkvparser::CuePoint* cp_int = cues_int->GetFirst();

  do {
    if (!cp || !cp_int)
      return false;

    if (cp->GetTimeCode() != cp_int->GetTimeCode())
      return false;

    // Check Block number
    const mkvparser::CuePoint::TrackPosition* const tp = cp->Find(track);
    const mkvparser::CuePoint::TrackPosition* const tp_int =
        cp_int->Find(track_int);
    if (tp && tp_int) {
      if (tp->m_block != tp_int->m_block)
        return false;
    }

    cp = cues->GetNext(cp);
    cp_int = cues_int->GetNext(cp_int);
  } while (cp);

  return true;
}

bool WebMFile::CheckCuesAlignmentList(
    const vector<const WebMFile*>& webm_list,
    double seconds,
    bool check_for_sap,
    bool check_for_audio_match,
    bool verbose,
    bool output_alignment_times,
    bool output_alignment_stats,
    string* output_string) {
  if (webm_list.size() < 2) {
    if (output_string)
      *output_string = "File list is less than 2.";
    return false;
  }

  vector<const mkvparser::Cues*> cues_list;
  vector<const mkvparser::Track*> video_track_list;
  vector<const mkvparser::Track*> audio_track_list;
  bool have_audio_stream = true;
  string negate_alignment;
  string alignment_times("|Align timecodes ");
  string alignment_stats("|Align stats ");

  if (output_string)
    *output_string = "Unknown";

  const mkvparser::SegmentInfo* const golden_info =
      webm_list.at(0)->GetSegmentInfo();

  // Setup the lists of cues and tracks.
  for (WebMConstIterator c_webm_iter = webm_list.begin(),
                         c_webm_iter_end = webm_list.end();
       c_webm_iter != c_webm_iter_end;
       ++c_webm_iter) {
    const WebMFile* const webm = *c_webm_iter;
    if (golden_info->GetTimeCodeScale() !=
        webm->GetSegmentInfo()->GetTimeCodeScale()) {
      if (output_string) {
        char str[1024];
        snprintf(str, sizeof(str),
                 "Timecode scales do not match. timecode_scale:%lld"
                 " timecode_scale:%lld",
                 golden_info->GetTimeCodeScale(),
                 webm->GetSegmentInfo()->GetTimeCodeScale());
        *output_string = str;
      }
      return false;
    }

    const mkvparser::Cues* const cues = webm->GetCues();
    if (!cues)
      return false;
    const mkvparser::Track* const track = webm->GetVideoTrack();
    if (!track)
      return false;
    const mkvparser::Track* const aud_track = webm->GetAudioTrack();
    if (!aud_track)
      have_audio_stream = false;

    cues_list.push_back(cues);
    video_track_list.push_back(track);
    if (have_audio_stream)
      audio_track_list.push_back(aud_track);
  }

  // Find minimum Cluster time across all files.
  int64 time = LLONG_MAX;
  for (CuesConstIterator c_cues_iter = cues_list.begin(),
                         c_cues_iter_end = cues_list.end();
       c_cues_iter != c_cues_iter_end;
       ++c_cues_iter) {
    const mkvparser::Cues* const cues = *c_cues_iter;
    const mkvparser::CuePoint* const cp = cues->GetFirst();
    if (!cp)
      return false;

    const int64 cue_time = cp->GetTimeCode();
    if (cue_time < time)
      time = cue_time;

    if (time == 0)
      break;
  }

  for (int64 last_alignment = time,
             no_alignment_timecode = time +
                 static_cast<int64>((seconds * kNanosecondsPerSecond) /
                                    golden_info->GetTimeCodeScale()); ; ) {
    // Check if all cues have the same alignment.
    bool found_alignment = true;

    typedef vector<const mkvparser::Cues*>::size_type cues_size_type;
    cues_size_type cues_list_size = cues_list.size();
    for (cues_size_type i = 0; i < cues_list_size; ++i) {
      const mkvparser::Cues* const cues = cues_list.at(i);
      const mkvparser::Track* const track = video_track_list.at(i);
      const mkvparser::CuePoint::TrackPosition* tp = NULL;
      const int64 nano = time * golden_info->GetTimeCodeScale();
      const mkvparser::CuePoint* cp = NULL;

      // Find the CuePoint for |nano|.
      if (!cues->Find(nano, track, cp, tp)) {
        const WebMFile* const file = webm_list.at(i);
        if (output_string) {
          char str[1024];
          snprintf(str, sizeof(str),
                   "Could not find CuePoint time:%lld track:%ld file:%s",
                   time, track->GetNumber(), file->filename().c_str());
          *output_string = str;
        }
        return false;
      }

      // Check if we went past our allotted time.
      if (cp->GetTimeCode() > no_alignment_timecode) {
        const WebMFile* const file = webm_list.at(i);
        if (output_string) {
          char str[4096];
          snprintf(str, sizeof(str),
                   "Could not find alignment in allotted time. seconds:%g"
                   " last_alignment:%lld cp time:%lld track:%ld file:%s",
                   seconds, last_alignment, cp->GetTimeCode(),
                   track->GetNumber(), file->filename().c_str());
          *output_string = str;
          *output_string += negate_alignment;
        }
        return false;
      }

      // Check if a CuePoint does not match.
      if (cp->GetTimeCode() != time) {
        const WebMFile* const file = webm_list.at(i);
        if (verbose) {
          printf("No alignment found at time:%lld cp time:%lld track:%ld"
                 " file:%s\n",
                 time, cp->GetTimeCode(), track->GetNumber(),
                 file->filename().c_str());
        }

        // Update time to check to this Cue's next timecode.
        found_alignment = false;
        break;
      }
    }

    if (verbose && found_alignment)
      printf("Potential alignment at time:%lld -- ", time);

    // Check if all of the cues start with a key frame.
    if (found_alignment && check_for_sap) {
      for (cues_size_type i = 0; i < cues_list_size; ++i) {
        const mkvparser::Cues* const cues = cues_list.at(i);
        const mkvparser::Track* const track = video_track_list.at(i);
        const mkvparser::CuePoint::TrackPosition* tp = NULL;
        const mkvparser::CuePoint* cp = NULL;
        const int64 nano = time * golden_info->GetTimeCodeScale();
        if (!cues->Find(nano, track, cp, tp))
          return false;

        const WebMFile* const file = webm_list.at(i);
        const mkvparser::Block* block = NULL;
        const mkvparser::Cluster* cluster = NULL;

        // Get the first block.
        if (!file->GetIndexedBlock(*cp, *track, 0, &cluster, &block))
          return false;

        if (!file->StartsWithKey(*cp, *cluster, *block)) {
          const bool altref = file->IsFrameAltref(*block);
          const int64 nano = block->GetTime(cluster);
          char str[4096];
          snprintf(str, sizeof(str),
                   " |!Key nano:%lld sec:%g altref:%s track:%ld file:%s",
                   nano, nano / kNanosecondsPerSecond,
                   altref ? "true" : "false",
                   track->GetNumber(),
                   file->filename().c_str());
          negate_alignment += str;
          if (verbose)
            printf("%s\n", str);

          if (output_string && output_alignment_stats) {
            snprintf(str, sizeof(str), "!Key:%lld,", nano / 1000000ULL);
            alignment_stats += str;
          }
          found_alignment = false;
          break;
        }
      }
    }

    // Check if all of the audio data matches on an alignment.
    if (have_audio_stream && found_alignment && check_for_audio_match) {
      const int64 nano = time * golden_info->GetTimeCodeScale();
      const WebMFile* const gold_file = webm_list.at(0);
      const mkvparser::Cues* const gold_cues = cues_list.at(0);
      const mkvparser::Track* const gold_audio = audio_track_list.at(0);
      const mkvparser::Track* const gold_video = video_track_list.at(0);
      const mkvparser::CuePoint* cp = NULL;
      const mkvparser::CuePoint::TrackPosition* tp = NULL;
      if (!gold_cues->Find(nano, gold_video, cp, tp))
        return false;

      // Get the first audio block time.
      int64 gold_time = 0;
      if (!gold_file->GetFirstBlockTime(*cp,
                                        gold_audio->GetNumber(),
                                        &gold_time))
        return false;

      for (cues_size_type i = 1; i < cues_list_size; ++i) {
        const mkvparser::Cues* const cues = cues_list.at(i);
        const mkvparser::Track* const aud_track = audio_track_list.at(i);
        const mkvparser::Track* const vid_track = video_track_list.at(i);
        if (!cues->Find(nano, vid_track, cp, tp))
          return false;

        int64 audio_time = 0;
        const WebMFile* const file = webm_list.at(i);
        if (!file->GetFirstBlockTime(*cp, aud_track->GetNumber(), &audio_time))
          return false;

        if (gold_time != audio_time) {
          char str[4096];
          snprintf(str, sizeof(str),
                   " |!Audio time_g:%lld time:%lld file_g:%s file:%s",
                   gold_time, audio_time, gold_file->filename().c_str(),
                   file->filename().c_str());
          negate_alignment += str;

          if (verbose)
            printf("%s\n", str);

          if (output_string && output_alignment_stats) {
            snprintf(str, sizeof(str), "!Audio:%lld,",
                     audio_time / 1000000ULL);
            alignment_stats += str;
          }

          found_alignment = false;
          break;
        }
      }
    }

    // Find minimum time after |time| across all files.
    int64 minimum_time = LLONG_MAX;
    for (cues_size_type i = 0; i < cues_list_size; ++i) {
      const mkvparser::Cues* const cues = cues_list.at(i);
      const mkvparser::Track* const track = video_track_list.at(i);
      const mkvparser::CuePoint* cp = NULL;
      const mkvparser::CuePoint::TrackPosition* tp = NULL;
      const int64 nano = time * golden_info->GetTimeCodeScale();
      if (!cues->Find(nano, track, cp, tp))
        return false;

      cp = cues->GetNext(cp);
      if (cp && cp->GetTimeCode() < minimum_time)
        minimum_time = cp->GetTimeCode();
    }

    if (minimum_time == LLONG_MAX) {
      if (output_string) {
        if (output_alignment_stats)
          *output_string = alignment_stats;
        else if (output_alignment_times)
          *output_string = alignment_times;
      }

      if (verbose)
        printf("Could not find next CuePoint assume files are done.\n");
      break;
    }

    if (found_alignment) {
      if (verbose)
        printf("Found alignment at time:%lld\n", time);
      if (output_string) {
        if (output_alignment_stats) {
          char str[4096];
          snprintf(str, sizeof(str), "Time:%lld", time);
          if (time)
            alignment_stats += ",";
          alignment_stats += str;
        } else if (output_alignment_times) {
          char str[4096];
          snprintf(str, sizeof(str), "%lld", time);
          if (time)
            alignment_times += ",";
          alignment_times += str;
        }
      }
      no_alignment_timecode = time +
          static_cast<int64>((seconds * kNanosecondsPerSecond) /
                             golden_info->GetTimeCodeScale());
      last_alignment = time;
      negate_alignment.clear();
    }

    time = minimum_time;
  }

  return true;
}

bool WebMFile::CheckForCues() const {
  if (!segment_.get())
    return false;

  const mkvparser::Cues* const cues = segment_->GetCues();
  if (!cues)
    return false;

  // Load all the cue points
  while (!cues->DoneParsing()) {
    cues->LoadCuePoint();
  }

  // Get the first track. Shouldn't matter what track it is because the
  // tracks will be in separate files.
  const mkvparser::Track* const track = GetTrack(0);
  if (!track)
    return false;

  const mkvparser::CuePoint* cue;
  const mkvparser::CuePoint::TrackPosition* track_position;

  // Check for the first cue.
  return cues->Find(0, track, cue, track_position);
}

bool WebMFile::CuesFirstInCluster(TrackTypes type) const {
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

int64 WebMFile::FileAverageBitsPerSecond() const {
  const int64 file_length = FileLength();
  const int64 duration = GetDurationNanoseconds();

  return static_cast<int64>(8.0 * file_length /
                            (duration / kNanosecondsPerSecond));
}

int64 WebMFile::FileLength() const {
  if (!reader_.get())
    return 0;
  int64 total;
  int64 available;
  if (reader_->Length(&total, &available) < 0)
    return 0;
  return total;
}

int64 WebMFile::FileMaximumBitsPerSecond() const {
  const mkvparser::Cues* const cues = GetCues();
  if (!cues)
    return 0;

  const mkvparser::CuePoint* cp = cues->GetFirst();
  int64 max_bps = 0;
  while (cp) {
    const int64 bps = CalculateBitsPerSecond(cp);
    if (bps > max_bps)
      max_bps = bps;

    cp = cues->GetNext(cp);
  }

  return max_bps;
}

string WebMFile::GetCodec() const {
  const string vorbis_id("A_VORBIS");
  const string vp8_id("V_VP8");
  string codec;

  const mkvparser::Track* track = GetTrack(0);
  if (track) {
    const string codec_id(track->GetCodecId());
    if (codec_id == vorbis_id)
      codec = "vorbis";
    else if (codec_id == vp8_id)
      codec = "vp8";
  }

  track = GetTrack(1);
  if (track) {
    const string codec_id(track->GetCodecId());
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

  return codec;
}

const mkvparser::Cues* WebMFile::GetCues() const {
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

int64 WebMFile::GetDurationNanoseconds() const {
  if (!segment_.get() || !segment_->GetInfo())
    return 0;
  return segment_->GetInfo()->GetDuration();
}

void WebMFile::GetHeaderRange(int64* start, int64* end) const {
  if (start)
    *start = 0;
  if (end)
    *end = GetClusterRangeStart();
}

string WebMFile::GetMimeType() const {
  string mimetype("video/webm");
  const string codec = GetCodec();
  if (codec == "vorbis")
    mimetype = "audio/webm";

  return mimetype;
}

string WebMFile::GetMimeTypeWithCodec() const {
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

  return mimetype;
}

const mkvparser::SegmentInfo* WebMFile::GetSegmentInfo() const {
  if (!segment_.get())
    return NULL;
  return segment_->GetInfo();
}

int64 WebMFile::GetSegmentStartOffset() const {
  if (!segment_.get())
    return 0;
  return segment_->m_start;
}

bool WebMFile::OnlyOneStream() const {
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();

  if (!aud_track && !vid_track) {
    fprintf(stderr, "WebM file does not have an audio or video track.\n");
    return false;
  }

  if (aud_track && vid_track)
    return false;

  if (aud_track) {
    const string vorbis_id("A_VORBIS");
    const string codec_id(aud_track->GetCodecId());
    if (codec_id != vorbis_id) {
      fprintf(stderr, "Audio track does not match A_VORBIS. :%s\n",
              codec_id.c_str());
      return false;
    }
  }

  if (vid_track) {
    const string vp8_id("V_VP8");
    const string codec_id(vid_track->GetCodecId());
    if (codec_id != vp8_id) {
      fprintf(stderr, "Video track does not match V_VP8. :%s\n",
              codec_id.c_str());
      return false;
    }
  }

  return true;
}

int64 WebMFile::PeakBitsPerSecondOverFile(int64 prebuffer_ns) const {
  const mkvparser::Cues* const cues = GetCues();
  if (!cues)
    return 0;

  const mkvparser::CuePoint* cp = cues->GetFirst();
  double max_bps = 0.0;
  while (cp) {
    const int64 start_nano = cp->GetTime(segment_.get());
    double bps = 0.0;
    const int rv = PeakBitsPerSecond(start_nano, prebuffer_ns, &bps);
    if (rv < 0)
      return rv;

    if (bps > max_bps)
      max_bps = bps;

    cp = cues->GetNext(cp);
  }

  return static_cast<int64>(max_bps);
}

int64 WebMFile::TrackAverageBitsPerSecond(TrackTypes type) const {
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
    return 0;

  return CalculateTrackBitsPerSecond(track->GetNumber(), NULL);
}

int64 WebMFile::TrackCount(TrackTypes type) const {
  if (!segment_.get())
    return 0;

  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  if (!tracks)
    return 0;

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

  return track_count;
}

int64 WebMFile::TrackFrameCount(TrackTypes type) const {
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
    return 0;

  return CalculateTrackFrameCount(track->GetNumber(), NULL);
}

int64 WebMFile::TrackSize(TrackTypes type) const {
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
    return 0;

  return CalculateTrackSize(track->GetNumber(), NULL);
}

int64 WebMFile::TrackStartNanoseconds(TrackTypes type) const {
  if (!calculated_file_stats_)
    return 0;

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
    return 0;

  return tracks_start_milli_.find(track->GetNumber())->second;
}

bool WebMFile::HasVideo() const {
  return (GetVideoTrack() != NULL);
}

double WebMFile::VideoFramerate() const {
  double rate = 0.0;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track) {
    rate = vid_track->GetFrameRate();
  }

  return rate;
}

int WebMFile::VideoHeight() const {
  int height = 0;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track) {
    height = static_cast<int>(vid_track->GetHeight());
  }

  return height;
}

int WebMFile::VideoWidth() const {
  int width = 0;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track) {
    width = static_cast<int>(vid_track->GetWidth());
  }

  return width;
}

int64 WebMFile::CalculateBitsPerSecond(const mkvparser::CuePoint* cp) const {
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

double WebMFile::CalculateFrameRate(int track_number) const {
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

int64 WebMFile::CalculateTrackBitsPerSecond(
    int track_number,
    const mkvparser::CuePoint* cp) const {
  int64 size = 0;
  int64 start_ns = 0;
  if (cp == NULL) {
    if (!calculated_file_stats_)
      return 0;

    size = tracks_size_.find(track_number)->second;
  } else {
    const mkvparser::Cluster* cluster =
        segment_->FindCluster(cp->GetTime(segment_.get()));
    if (!cluster)
      return 0;

    start_ns = cluster->GetTime();
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

  const int64 duration = GetDurationNanoseconds() - start_ns;
  const double bitrate = (size * 8) / (duration / kNanosecondsPerSecond);
  return static_cast<int64>(bitrate);
}

int64 WebMFile::CalculateTrackFrameCount(int track_number,
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

int64 WebMFile::CalculateTrackSize(int track_number,
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

bool WebMFile::CheckDocType(const string& doc_type) const {
  return (doc_type.compare(0, 4, "webm") == 0);
}

void WebMFile::FindCuesChunk(int64 start_time_nano,
                             int64 end_time_nano,
                             int64* start,
                             int64* end,
                             int64* cue_start_time,
                             int64* cue_end_time) const {
  if (!start || !end || !cue_start_time || !cue_end_time || !segment_.get())
    return;

  *start = 0;
  *end = 0;
  *cue_start_time = 0;
  *cue_end_time = 0;

  const mkvparser::Cues* const cues = GetCues();
  if (cues) {
    const mkvparser::CuePoint* cue;
    const mkvparser::CuePoint::TrackPosition* track_position;

    // Get the first track. Shouldn't matter what track it is because the
    // tracks will be in separate files.
    const mkvparser::Track* const track = GetTrack(0);

    bool b = cues->Find(start_time_nano, track, cue, track_position);
    if (b) {
      int64 time_nano;

      while ((time_nano = cue->GetTime(segment_.get())) < start_time_nano) {
        cue = cues->GetNext(cue);
        if (!cue)
          return;  // reached eof
      }

      *start = cue->m_element_start;
      *cue_start_time = cue->GetTime(segment_.get());

      const mkvparser::CuePoint* cue_prev = cue;

      while ((time_nano = cue->GetTime(segment_.get())) < end_time_nano) {
        cue_prev = cue;
        cue = cues->GetNext(cue);
        if (!cue) {
          // We have reached eof. Set our current cue to our previous cue so
          // our end time will not include the duration of the last cue.
          cue = cue_prev;
          break;
        }
      }

      *end = cue_prev->m_element_start + cue_prev->m_element_size;
      *cue_end_time = cue->GetTime(segment_.get());
    }
  }
}

bool WebMFile::GenerateStats() {
  if (calculated_file_stats_)
    return true;

  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  if (!tracks)
    return 0;

  std::pair<std::map<int, int64>::iterator, bool> ret;
  const int32 track_count = static_cast<int32>(tracks->GetTracksCount());
  for (int i = 0; i < track_count; ++i) {
    const mkvparser::Track* const track = tracks->GetTrackByIndex(i);
    const int track_number = track->GetNumber();

    ret = tracks_size_.insert(std::pair<int, int64>(track_number, 0));
    if (!ret.second)
      return false;
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
    if (status)
      return false;

    while (block_entry && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      const int track_number = block->GetTrackNumber();

      tracks_size_[track_number] += block->m_size;
      tracks_frame_count_[track_number]++;
      if (tracks_start_milli_[track_number] == -1)
        tracks_start_milli_[track_number] = block->GetTime(cluster);

      status = cluster->GetNext(block_entry, block_entry);
      if (status)
        return false;
    }

    cluster = segment_->GetNext(cluster);
  }

  calculated_file_stats_ = true;
  return true;
}

const mkvparser::AudioTrack* WebMFile::GetAudioTrack() const {
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

int64 WebMFile::GetClusterRangeStart() const {
  if (!segment_.get())
    return -1;

  const mkvparser::Cluster* const cluster = segment_->GetFirst();

  int64 start = -1;
  if (cluster) {
    start = cluster->m_element_start;
  }

  return start;
}

bool WebMFile::GetFirstBlockTime(const mkvparser::CuePoint& cp,
                                 int track_num,
                                 int64* nanoseconds) const {
  if (!nanoseconds)
    return false;

  // Find the first block that matches the CuePoint track number. This is
  // done because the block number may not be set which defaults to 1.
  const mkvparser::Cluster* const cluster =
      segment_->FindCluster(cp.GetTime(segment_.get()));
  if (!cluster)
    return false;

  const mkvparser::BlockEntry* block_entry;
  int status = cluster->GetFirst(block_entry);
  if (status)
    return false;

  while (block_entry && !block_entry->EOS()) {
    const mkvparser::Block* const block = block_entry->GetBlock();
    if (block->GetTrackNumber() == track_num) {
      *nanoseconds = block->GetTime(cluster);
      return true;
    }

    status = cluster->GetNext(block_entry, block_entry);
    if (status)
      return false;
  }

  return false;
}

const CueDesc* WebMFile::GetCueDescFromTime(int64 time) const {
  if (!segment_.get())
    return NULL;

  const mkvparser::Cues* const cues = GetCues();
  if (!cues)
    return NULL;

  int l = 0;
  int r = cue_desc_list_.size() - 1;
  if (time >= cue_desc_list_[r].start_time_ns)
    l = r;

  while (l + 1 < r) {
    int m = l + static_cast<int>(((r - l) / 2.0) + 0.5);
    int64 timestamp = cue_desc_list_[m].start_time_ns;
    if (timestamp <= time) {
      l = m;
    } else {
      r = m;
    }
  }

  const CueDesc* const desc = &cue_desc_list_[l];

  // Make sure time is not EOF
  if (time < desc->end_time_ns) {
    return desc;
  }
  return NULL;
}

bool WebMFile::GetIndexedBlock(const mkvparser::CuePoint& cp,
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

void WebMFile::GetSegmentInfoRange(int64* start, int64* end) const {
  if (!start || !end)
    return;

  *start = 0;
  *end = 0;

  if (!segment_.get())
    return;

  const mkvparser::SegmentInfo* const segment_info = segment_->GetInfo();
  if (segment_info) {
    *start = segment_info->m_element_start;
    *end = segment_info->m_element_start + segment_info->m_element_size;
  }
}

const mkvparser::Track* WebMFile::GetTrack(uint32 index) const {
  if (!segment_.get())
    return NULL;

  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  if (!tracks)
    return NULL;

  return tracks->GetTrackByIndex(index);
}

void WebMFile::GetTracksRange(int64* start, int64* end) const {
  if (!start || !end)
    return;

  *start = 0;
  *end = 0;

  if (!segment_.get())
    return;

  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  if (tracks) {
    *start = tracks->m_element_start;
    *end = tracks->m_element_start + tracks->m_element_size;
  }
}

const mkvparser::VideoTrack* WebMFile::GetVideoTrack() const {
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

bool WebMFile::LoadCueDescList() {
  if (!segment_.get())
    return false;

  const mkvparser::Cues* const cues = GetCues();
  if (!cues)
    return false;

  // Get the first track
  const mkvparser::Track* const track = GetTrack(0);
  if (!track)
    return false;
  const mkvparser::CuePoint* cp = cues->GetFirst();

  int64 last_time_ns = -1;
  int64 last_offset = -1;

  while (cp) {
    const int64 time = cp->GetTime(segment_.get());

    const mkvparser::CuePoint::TrackPosition* const tp = cp->Find(track);
    if (!tp)
      return false;
    const int64 offset = tp->m_pos;

    if (last_time_ns != -1) {
      CueDesc desc;
      desc.start_time_ns = last_time_ns;
      desc.end_time_ns = time;
      desc.start_offset = last_offset;
      desc.end_offset = offset;
      cue_desc_list_.push_back(desc);
    }

    last_time_ns = time;
    last_offset = offset;

    cp = cues->GetNext(cp);
  }

  if (last_time_ns != -1) {
    CueDesc desc;
    desc.start_time_ns = last_time_ns;
    desc.end_time_ns = segment_->GetInfo()->GetDuration();
    desc.start_offset = last_offset;
    desc.end_offset = cues->m_element_start;
    cue_desc_list_.push_back(desc);
  }

  return true;
}

bool WebMFile::IsFrameAltref(const mkvparser::Block& block) const {
  // Only check the first frame.
  const mkvparser::Block::Frame& frame = block.GetFrame(0);

  if (frame.len > 0) {
    vector<unsigned char> vector_data;
    vector_data.resize(frame.len);
    unsigned char* data = &vector_data[0];
    if (data) {
      if (!frame.Read(reader_.get(), data)) {
        return (data[0] >> 4) & 1;
      }
    }
  }

  return false;
}

int WebMFile::PeakBitsPerSecond(int64 time_ns,
                                int64 prebuffer_ns,
                                double* bits_per_second) const {
  if (!bits_per_second)
    return -1;

  const CueDesc* const desc_beg = GetCueDescFromTime(time_ns);
  if (!desc_beg) {
    fprintf(
        stderr,
        "PeakBitsPerSecond() GetCueDescFromTime returned NULL. time_ns:%lld\n",
        time_ns);
    return -1;
  }

  // TODO(fgalligan): Handle non-cue start time.
  if (desc_beg->start_time_ns != time_ns) {
    fprintf(stderr,
            "PeakBitsPerSecond() CueDesc time != time_ns. time:%lld"
            " time_ns:%lld\n",
            desc_beg->start_time_ns, time_ns);
    return -1;
  }

  const int64 prebuffered_ns = time_ns + prebuffer_ns;
  double prebuffer_bytes = 0.0;
  int64 temp_prebuffer_ns = prebuffer_ns;

  // Start with the first Cue.
  const CueDesc* desc_end = desc_beg;

  // Figure out how much data we have downloaded for the prebuffer. This will
  // be used later to adjust the bits per sample to try.
  while (desc_end && desc_end->end_time_ns < prebuffered_ns) {
    // Prebuffered the entire Cue.
    prebuffer_bytes += desc_end->end_offset - desc_end->start_offset;
    temp_prebuffer_ns -= desc_end->end_time_ns - desc_end->start_time_ns;
    desc_end = GetCueDescFromTime(desc_end->end_time_ns);
  }

  if (!desc_end) {
    // The prebuffer is larger than the duration.
    *bits_per_second = 0.0;
    if (segment_->GetInfo()->GetDuration() >= prebuffered_ns)
      return -1;
    return 0;
  }

  // The prebuffer ends in the last Cue. Estimate how much data was
  // prebuffered.
  const int64 pre_bytes = desc_end->end_offset - desc_end->start_offset;
  const int64 pre_ns = desc_end->end_time_ns - desc_end->start_time_ns;
  const double pre_sec = pre_ns / kNanosecondsPerSecond;
  prebuffer_bytes +=
      pre_bytes * ((temp_prebuffer_ns / kNanosecondsPerSecond) / pre_sec);

  const double prebuffer = prebuffer_ns / kNanosecondsPerSecond;
  // Set this to 0.0 in case our prebuffer buffers the entire video.
  *bits_per_second = 0.0;
  do {
    const int64 desc_bytes = desc_end->end_offset - desc_beg->start_offset;
    const int64 desc_ns = desc_end->end_time_ns - desc_beg->start_time_ns;
    const double desc_sec = desc_ns / kNanosecondsPerSecond;
    const double calc_bits_per_second = (desc_bytes * 8) / desc_sec;

    // Drop the bps by the percentage of bytes buffered.
    const double percent = (desc_bytes - prebuffer_bytes) / desc_bytes;
    const double mod_bits_per_second = calc_bits_per_second * percent;

    if (prebuffer < desc_sec) {
      const double search_sec =
          static_cast<double>(segment_->GetInfo()->GetDuration()) /
                              kNanosecondsPerSecond;

      // Add 1 so the bits per second should be a little bit greater than file
      // datarate.
      const int64 bps = static_cast<int64>(mod_bits_per_second) + 1;
      const double min_buffer = 0.0;
      double buffer = prebuffer;
      double sec_to_download = 0.0;
      const int rv = BufferSizeAfterTimeDownloaded(prebuffered_ns,
                                                   search_sec,
                                                   bps,
                                                   min_buffer,
                                                   &buffer,
                                                   &sec_to_download);
      if (rv < 0) {
        fprintf(stderr,
                "PeakBitsPerSecond() BufferSizeAfterTimeDownloaded rv:%d\n",
                rv);
        return rv;
      } else if (rv == 0) {
        *bits_per_second = static_cast<double>(bps);
        break;
      }
    }

    desc_end = GetCueDescFromTime(desc_end->end_time_ns);
  } while (desc_end);

  return 0;
}

bool WebMFile::StartsWithKey(const mkvparser::CuePoint& cp,
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
