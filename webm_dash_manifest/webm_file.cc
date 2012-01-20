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

#include <cstring>

#include "indent.h"
#include "mkvreader.hpp"

using indent_webm::Indent;
using std::string;
using std::vector;

namespace webm_dash {

WebMFile::WebMFile(const string& filename)
    : cue_chunk_time_nano_(0x7FFFFFFFFFFFFFFF),
      filename_(filename) {
}

WebMFile::~WebMFile() {
}

bool WebMFile::Init() {
  reader_.reset(new mkvparser::MkvReader());

  if (reader_->Open(filename_.c_str())) {
    printf("Error trying to open file:%s\n", filename_.c_str());
    return false;
  }

  long long pos = 0;

  ebml_header_.reset(new mkvparser::EBMLHeader());
  ebml_header_->Parse(reader_.get(), pos);

  if (!CheckDocType()) {
    printf("DocType != webm\n");
    return false;
  }

  mkvparser::Segment* segment;
  if (mkvparser::Segment::CreateInstance(reader_.get(), pos, segment)) {
    printf("Segment::CreateInstance() failed.\n");
    return false;
  }

  segment_.reset(segment);
  if (segment_->Load() < 0) {
    printf("Segment::Load() failed.\n");
    return false;
  }

  if (!LoadCueDescList()) {
    printf("LoadCueDescList() failed.\n");
    return false;
  }

  return true;
}

int WebMFile::BufferSizeAfterTime(double time,
                                  double search_sec,
                                  long long kbps,
                                  double* buffer,
                                  double* sec_counted) const {
  if (!buffer)
    return -1;

  const long long time_ns = static_cast<long long>(time * 1000000000.0);

  const CueDesc* descCurr = GetCueDescFromTime(time_ns);
  if (!descCurr)
    return -1;

  // TODO(fgalligan): Handle non-cue start time.
  if (descCurr->start_time_ns != time_ns)
    return -1;

  double sec_downloading = 0.0;
  double sec_downloaded = 0.0;

  do {
    const long long desc_bytes = descCurr->end_offset - descCurr->start_offset;
    const double desc_sec =
        (descCurr->end_time_ns - descCurr->start_time_ns) / 1000000000.0;
    const double time_to_download = ((desc_bytes * 8) / 1000.0) / kbps;

    sec_downloading += time_to_download;
    sec_downloaded += desc_sec;

    if (sec_downloading > search_sec) {
      sec_downloaded = (search_sec / sec_downloading) * sec_downloaded;
      sec_downloading = search_sec;
      break;
    }

    descCurr = GetCueDescFromTime(descCurr->end_time_ns);

  } while(descCurr);

  *buffer = sec_downloaded - sec_downloading + *buffer;
  if (sec_counted)
    *sec_counted = sec_downloading;

  return 0;
};

int WebMFile::BufferSizeAfterTimeDownloaded(long long time_ns,
                                            double search_sec,
                                            long long bps,
                                            double min_buffer,
                                            double* buffer,
                                            double* sec_to_download) const {
  if (!buffer || !sec_to_download)
    return -1;

  const double time_sec = time_ns / 1000000000.0;

  const CueDesc* descCurr = GetCueDescFromTime(time_ns);
  if (!descCurr)
    return -1;

  int rv = 0;
  const long long time_to_search_ns =
      static_cast<long long>(search_sec * 1000000000.0);
  const long long end_time_ns = time_ns + time_to_search_ns;
  *sec_to_download = 0.0;
  double sec_downloaded = 0.0;

  // Check for non cue start time.
  if (time_ns > descCurr->start_time_ns) {
    const long long cue_nano = descCurr->end_time_ns - time_ns;
    const double percent =
        static_cast<double>(cue_nano) /
        (descCurr->end_time_ns - descCurr->start_time_ns);
    const double cueBytes =
        (descCurr->end_offset - descCurr->start_offset) * percent;
    const double timeToDownload = (cueBytes * 8.0) / bps;

    sec_downloaded += (cue_nano / 1000000000.0) - timeToDownload;
    *sec_to_download += timeToDownload;

    // Check if the search ends within the first cue.
    if (descCurr->end_time_ns >= end_time_ns) {
      const double desc_end_time_sec = descCurr->end_time_ns / 1000000000.0;
      const double percent_to_sub =
          search_sec / (desc_end_time_sec - time_sec);
      sec_downloaded = percent_to_sub * sec_downloaded;
      *sec_to_download = percent_to_sub * *sec_to_download;
    }

    if ( (sec_downloaded + *buffer) <= min_buffer) {
      return 1;
    }

    // Get the next Cue.
    descCurr = GetCueDescFromTime(descCurr->end_time_ns);
  }

  while (descCurr) {
    const long long desc_bytes = descCurr->end_offset - descCurr->start_offset;
    const long long desc_ns = descCurr->end_time_ns - descCurr->start_time_ns;
    const double desc_sec = desc_ns / 1000000000.0;
    const double bits = (desc_bytes * 8.0);
    const double time_to_download = bits / bps;

    sec_downloaded += desc_sec - time_to_download;
    *sec_to_download += time_to_download;

    if (descCurr->end_time_ns >= end_time_ns) {
      const double desc_end_time_sec = descCurr->end_time_ns / 1000000000.0;
      const double percent_to_sub =
          search_sec / (desc_end_time_sec - time_sec);
      sec_downloaded = percent_to_sub * sec_downloaded;
      *sec_to_download = percent_to_sub * *sec_to_download;

      if ( (sec_downloaded + *buffer) <= min_buffer)
        rv = 1;
      break;
    }

    if ( (sec_downloaded + *buffer) <= min_buffer) {
      rv = 1;
      break;
    }

    descCurr = GetCueDescFromTime(descCurr->end_time_ns);
  };

  *buffer = *buffer + sec_downloaded;

  return rv;
};

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
  const unsigned char* codec_priv = track->GetCodecPrivate(size);
  const unsigned char* codec_priv_int = track_int->GetCodecPrivate(size_int);
  if (size != size_int)
    return false;

  if (memcmp(codec_priv, codec_priv_int, size))
    return false;

  return true;
}

bool WebMFile::CheckCuesAlignement(const WebMFile& webm_file) const {
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
  } while(cp != NULL);

  return true;
}

bool WebMFile::CheckForCues() const {
  if (!segment_.get())
    return false;

  bool b = false;
  const mkvparser::Cues* const cues = segment_->GetCues();
  if (cues) {
    // Load all the cue points
    while (!cues->DoneParsing()) {
      cues->LoadCuePoint();
    }

    const mkvparser::CuePoint* cue;
    const mkvparser::CuePoint::TrackPosition* track_position;

    // Get the first track. Shouldn't matter what track it is because the
    // tracks will be in separate files.
    const mkvparser::Track* const track = GetTrack(0);

    // Check for the first cue.
    b = cues->Find(0, track, cue, track_position);
  }

  return b;
}

bool WebMFile::CuesFirstInCluster() const {
  const mkvparser::Cues* const cues = GetCues();
  if (!cues)
    return false;

  const mkvparser::Track* const track = GetTrack(0);
  if (!track)
    return false;

  const mkvparser::CuePoint* cp = cues->GetFirst();

  do {
    if (!cp)
      return false;

    // Check Block number
    const mkvparser::CuePoint::TrackPosition* const tp = cp->Find(track);
    if (!tp)
      return false;

    if (tp->m_block != 1)
        return false;

    // Find the first block that matches the CuePoint track number. This is
    // done because the block number may not be set which defaults to 1.
    const mkvparser::Cluster* cluster =
        segment_->FindCluster(cp->GetTime(segment_.get()));
    if (!cluster)
      return false;

    const mkvparser::BlockEntry* block_entry = cluster->GetFirst();
    while (block_entry != NULL && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      if (block->GetTrackNumber() == tp->m_track) {
        // Check if the block is a key frame and check if the block has the
        // same time as the cue point.
        if (!block->IsKey())
          return false;

        if (block->GetTime(cluster) != cp->GetTime(segment_.get()))
          return false;

        break;
      }

      block_entry = cluster->GetNext(block_entry);
    }

    cp = cues->GetNext(cp);
  } while(cp != NULL);

  return true;
}

int WebMFile::GetAudioChannels() const {
  int channels = 0;
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  if (aud_track) {
    channels = static_cast<int>(aud_track->GetChannels());
  }

  return channels;
}

int WebMFile::GetAudioSampleRate() const {
  int sample_rate = 0;
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  if (aud_track) {
    sample_rate = static_cast<int>(aud_track->GetSamplingRate());
  }

  return sample_rate;
}

long long WebMFile::GetAverageBandwidth() const {
  return CalculateBandwidth(NULL);
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

long long WebMFile::GetDurationNanoseconds() const {
  if (!segment_.get() || !segment_->GetInfo())
    return 0;
  return segment_->GetInfo()->GetDuration();
}

void WebMFile::GetHeaderRange(long long* start, long long* end) const {
  if (start)
    *start = 0;
  if (end)
    *end = GetClusterRangeStart();
}

long long WebMFile::GetMaximumBandwidth() const {
  const mkvparser::Cues* const cues = GetCues();
  if (!cues)
    return 0;

  const mkvparser::CuePoint* cp = cues->GetFirst();
  long long max_bandwidth = 0;
  while(cp != NULL) {
    const long long bandwidth = CalculateBandwidth(cp);
    if (bandwidth > max_bandwidth)
      max_bandwidth = bandwidth;

    cp = cues->GetNext(cp);
  }

  return max_bandwidth;
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

long long WebMFile::GetSegmentStartOffset() const {
  if (!segment_.get())
    return 0;
  return segment_->m_start;
}

double WebMFile::GetVideoFramerate() const {
  double rate = 0.0;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track) {
    rate = vid_track->GetFrameRate();
  }

  return rate;
}

int WebMFile::GetVideoHeight() const {
  int height = 0;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track) {
    height = static_cast<int>(vid_track->GetHeight());
  }

  return height;
}

int WebMFile::GetVideoWidth() const {
  int width = 0;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track) {
    width = static_cast<int>(vid_track->GetWidth());
  }

  return width;
}

bool WebMFile::OnlyOneStream() const {
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();

  if (!aud_track && !vid_track) {
    printf("WebM file does not have an audio or video track.\n");
    return false;
  }

  if (aud_track && vid_track)
    return false;

  if (aud_track) {
    const string vorbis_id("A_VORBIS");
    const string codec_id(aud_track->GetCodecId());
    if (codec_id != vorbis_id) {
      printf("Audio track does not match A_VORBIS. :%s\n", codec_id.c_str());
      return false;
    }
  }

  if (vid_track) {
    const string vp8_id("V_VP8");
    const string codec_id(vid_track->GetCodecId());
    if (codec_id != vp8_id) {
      printf("Video track does not match V_VP8. :%s\n", codec_id.c_str());
      return false;
    }
  }

  return true;
}

long long WebMFile::PeakBandwidthOverFile(long long prebuffer_ns) const {
  const mkvparser::Cues* const cues = GetCues();
  if (!cues)
    return 0;

  const mkvparser::CuePoint* cp = cues->GetFirst();
  double max_bandwidth = 0.0;
  while(cp != NULL) {
    const long long start_nano = cp->GetTime(segment_.get());
    double bps = 0.0;
    const int rv = PeakBandwidth(start_nano, prebuffer_ns, &bps);
    if (rv < 0)
      return rv;

    if (bps > max_bandwidth)
      max_bandwidth = bps;

    cp = cues->GetNext(cp);
  }

  return static_cast<long long>(max_bandwidth);
}

long long WebMFile::CalculateBandwidth(const mkvparser::CuePoint* cp) const {
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

  long long filesize = 0;
  const long long start = cluster->GetTime();
  const long long start_offset = cluster->m_element_start;
  while ((cluster != NULL) && !cluster->EOS()) {
    if ((cluster->m_element_start + cluster->GetElementSize()) > filesize)
      filesize = cluster->m_element_start + cluster->GetElementSize();

    cluster = segment_->GetNext(cluster);
  }

  if (!segment_->GetInfo())
    return 0;

  filesize -= start_offset;
  const long long duration = segment_->GetInfo()->GetDuration() - start;
  if (duration <= 0)
    return 0;
  const long long bandwidth =
    static_cast<long long>((filesize * 8) / (duration / 1000000000.0) / 1000);
  return bandwidth;
}

double WebMFile::CalculateFrameRate(long long track_number) const {
  if (segment_->GetInfo()->GetDuration() == 0)
    return 0.0;

  int frames = 0;
  const mkvparser::Cluster* cluster = segment_->GetFirst();
  while ((cluster != NULL) && !cluster->EOS()) {
    const mkvparser::BlockEntry* block_entry = cluster->GetFirst();

    while ((block_entry != NULL) && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      if (track_number == block->GetTrackNumber())
        frames += block->GetFrameCount();

      block_entry = cluster->GetNext(block_entry);
    }

    cluster = segment_->GetNext(cluster);
  }

  const double seconds = segment_->GetInfo()->GetDuration() / 1000000000.0;
  const double frame_rate = frames / seconds;

  return frame_rate;
}

bool WebMFile::CheckDocType() const {
  bool type_is_webm = false;
  if (!ebml_header_.get())
    return false;
  const string doc_type(ebml_header_->m_docType);
  if (doc_type.compare(0, 4, "webm") == 0)
    type_is_webm = true;

  return type_is_webm;
}

void WebMFile::FindCuesChunk(long long start_time_nano,
                             long long end_time_nano,
                             long long* start,
                             long long* end,
                             long long* cue_start_time,
                             long long* cue_end_time) const {
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
      long long time_nano;

      while ( (time_nano = cue->GetTime(segment_.get())) < start_time_nano) {
        cue = cues->GetNext(cue);
        if (!cue)
          return; // reached eof
      }

      *start = cue->m_element_start;
      *cue_start_time = cue->GetTime(segment_.get());

      const mkvparser::CuePoint* cue_prev = cue;

      while ( (time_nano = cue->GetTime(segment_.get())) < end_time_nano) {
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

const mkvparser::AudioTrack* WebMFile::GetAudioTrack() const {
  if (!segment_.get())
    return NULL;

  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  if (!tracks)
    return NULL;

  unsigned long i = 0;
  const unsigned long j = tracks->GetTracksCount();

  // TODO: This should be an enum of mkvparser::Tracks
  enum { VIDEO_TRACK = 1, AUDIO_TRACK = 2 };

  while (i != j) {
    const mkvparser::Track* const track = tracks->GetTrackByIndex(i++);

    if (track == NULL)
      continue;

    if (track->GetType() == AUDIO_TRACK)
      return static_cast<const mkvparser::AudioTrack*>(track);
  }

  return NULL;
}

long long WebMFile::GetClusterRangeStart() const {
  if (!segment_.get())
    return -1;

  const mkvparser::Cluster* const cluster = segment_->GetFirst();

  long long start = -1;
  if (cluster) {
    start = cluster->m_element_start;
  }

  return start;
}

const CueDesc* WebMFile::GetCueDescFromTime(long long time) const {
  if (!segment_.get())
    return NULL;

  const mkvparser::Cues* const cues = GetCues();
  if (!cues)
    return NULL;

  int l = 0;
  int r = cue_desc_list_.size() - 1;
  if (time >= cue_desc_list_[r].start_time_ns)
    l = r;

  while(l + 1 < r) {
    int m = l + static_cast<int>(((r - l) / 2.0) + 0.5);
    long long timestamp = cue_desc_list_[m].start_time_ns;
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
};

void WebMFile::GetSegmentInfoRange(long long* start, long long* end) const {
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

const mkvparser::Track* WebMFile::GetTrack(unsigned int index) const {
  if (!segment_.get())
    return NULL;

  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  if (!tracks)
    return NULL;

  return tracks->GetTrackByIndex(index);
}

void WebMFile::GetTracksRange(long long* start, long long* end) const {
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

  unsigned long i = 0;
  const unsigned long j = tracks->GetTracksCount();

  // TODO: This should be an enum of mkvparser::Tracks
  enum { VIDEO_TRACK = 1, AUDIO_TRACK = 2 };

  while (i != j) {
    const mkvparser::Track* const track = tracks->GetTrackByIndex(i++);

    if (track == NULL)
      continue;

    if (track->GetType() == VIDEO_TRACK)
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

  long long last_time_ns = -1;
  long long last_offset = -1;

  while(cp != NULL) {
    const long long time = cp->GetTime(segment_.get());

    const mkvparser::CuePoint::TrackPosition* const tp = cp->Find(track);
    const long long offset = tp->m_pos;

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

int WebMFile::PeakBandwidth(long long time_ns,
                            long long prebuffer_ns,
                            double* bandwidth) const {
  if (!bandwidth)
    return -1;

  const CueDesc* const desc_beg = GetCueDescFromTime(time_ns);
  if (!desc_beg) {
    printf("PeakBandwidth() GetCueDescFromTime returned NULL. time_ns:%lld\n",
           time_ns);
    return -1;
  }

  // TODO(fgalligan): Handle non-cue start time.
  if (desc_beg->start_time_ns != time_ns) {
    printf("PeakBandwidth() CueDesc time != time_ns. time:%lld time_ns:%lld\n",
           desc_beg->start_time_ns, time_ns);
    return -1;
  }

  const long long prebuffered_ns = time_ns + prebuffer_ns;
  double prebuffer_bytes = 0.0;
  long long temp_prebuffer_ns = prebuffer_ns;

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
    *bandwidth = 0.0;
    if (segment_->GetInfo()->GetDuration() >= prebuffered_ns)
      return -1;
    return 0;
  }

  // The prebuffer ends in the last Cue. Esitmate how much data was
  // prebuffered.
  const long long pre_bytes = desc_end->end_offset - desc_end->start_offset;
  const long long pre_ns = desc_end->end_time_ns - desc_end->start_time_ns;
  const double pre_sec = pre_ns / 1000000000.0;
  prebuffer_bytes +=
      pre_bytes * ((temp_prebuffer_ns / 1000000000.0) / pre_sec);

  const double prebuffer = prebuffer_ns / 1000000000.0;
  // Set this to 0.0 in case our prebuffer buffers the entire video.
  *bandwidth = 0.0;
  do {
    const long long desc_bytes = desc_end->end_offset - desc_beg->start_offset;
    const long long desc_ns = desc_end->end_time_ns - desc_beg->start_time_ns;
    const double desc_sec = desc_ns / 1000000000.0;
    const double bits_per_second = (desc_bytes * 8) / desc_sec;

    // Drop the bps by the percentage of bytes buffered.
    const double percent = (desc_bytes - prebuffer_bytes) / desc_bytes;
    const double mod_bits_per_second = bits_per_second * percent;

    if (prebuffer < desc_sec) {
      const double search_sec =
          static_cast<double>(segment_->GetInfo()->GetDuration()) /
                              1000000000.0;

      // Add 1 so the bandwidth should be a little bit greater than file
      // datarate.
      const long long bps = static_cast<long long>(mod_bits_per_second) + 1;
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
        printf("PeakBandwidth() BufferSizeAfterTimeDownloaded rv:%d\n", rv);
        return rv;
      } else if (rv == 0) {
        *bandwidth = static_cast<double>(bps);
        break;
      }
    }

    desc_end = GetCueDescFromTime(desc_end->end_time_ns);
  } while(desc_end);

  return 0;
};

}  // namespace webm_dash
