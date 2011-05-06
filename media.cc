/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "media.h"

#include <assert.h>
#include <iostream>

#include "indent.h"
#include "mkvreader.hpp"

using std::cout;
using std::endl;
using indent_webm::Indent;

namespace adaptive_manifest {

Media::Media(const string& id)
    : cue_chunk_time_nano_(0x7FFFFFFFFFFFFFFF),
      id_(id),
      file_() {
}

Media::~Media() {
}

bool Media::Init() {
  reader_.reset(new MkvReader());

  if (reader_->Open(file_.c_str())) {
    cout << "Error trying to open file:" << file_ << endl;
    return false;
  }

  long long pos = 0;

  ebml_header_.reset(new mkvparser::EBMLHeader());
  ebml_header_->Parse(reader_.get(), pos);

  if (!CheckDocType()) {
    cout << "DocType != webm" << endl;
    return false;
  }
  
  mkvparser::Segment* segment;
  if (mkvparser::Segment::CreateInstance(reader_.get(), pos, segment)) {
    cout << "Segment::CreateInstance() failed." << endl;
    return false;
  }

  segment_.reset(segment);
  if (segment_->Load() < 0) {
    cout << "Segment::Load() failed." << endl;
    return false;
  }

  if (!CheckCodecTypes()) {
    cout << "Video codec_id != V_VP8 || Audio codec_id != A_VORBIS" << endl;
    return false;
  }

  return true;
}

string Media::GetCodec() const {
  string codec;
  const mkvparser::Track* const track = GetTrack(0);
  if (track) {
    string vorbis_id("A_VORBIS");
    string vp8_id("V_VP8");
    string codec_id(track->GetCodecId());

    if (codec_id == vorbis_id)
      codec = "vorbis";
    else if (codec_id == vp8_id)
      codec = "vp8";
  }

  return codec;
}

long long Media::GetDurationNanoseconds() const {
  assert(segment_.get()!=NULL);
  assert(segment_->GetInfo()!=NULL);
  return segment_->GetInfo()->GetDuration();
}

void Media::OutputPrototypeManifest(std::ostream& o, Indent& indt) {
  indt.Adjust(2);
  o << indt << "<media id=\"" << id_ << "\"";
  o << " bandwidth=\"" << GetAverageBandwidth() << "\"";
  o << " url=\"" << file_ << "\"";

  long long start;
  long long end;
  GetSegmentInfoRange(start, end);
  o << " media_range=\"" << start << "-" << end << "\"";
  
  GetTracksRange(start, end);
  o << " stream_range=\"" << start << "-" << end << "\"";

  // Video
  const int width = GetVideoWidth();
  if (width > 0) {
    o << " width=\"" << width << "\"";
  }
  const int height = GetVideoHeight();
  if (height > 0) {
    o << " height=\"" << height << "\"";
  }
  const double rate = GetVideoFramerate();
  if (rate > 0.0) {
    o << " framerate=\"" << rate << "\"";
  }

  // Audio
  const int channels = GetAudioChannels();
  if (channels > 0) {
    o << " channels=\"" << channels << "\"";
  }
  const int samplerate = GetAudioSampleRate();
  if (samplerate > 0) {
    o << " samplerate=\"" << samplerate << "\"";
  }

  o << " >" << endl;

  OutputPrototypeManifestCues(o, indt);

  o << indt << "</media>" << endl;

  indt.Adjust(-2);
}

bool Media::CheckDocType() const {
  bool type_is_webm = false;

  assert(ebml_header_.get()!=NULL);
  string doc_type(ebml_header_->m_docType);
  if (doc_type.compare(0,4,"webm") == 0)
    type_is_webm = true;

  return type_is_webm;
}

bool Media::CheckCodecTypes() const {
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();

  if (!aud_track && !vid_track) {
    cout << "WebM file does not have an audio or video track." << endl;
    return false;
  }

  if (aud_track && vid_track)
    return false;

  if (aud_track) {
    string vorbis_id("A_VORBIS");
    string codec_id(aud_track->GetCodecId());
    if (codec_id != vorbis_id) {
      cout << "Audio track does not match A_VORBIS. :" << codec_id << endl;
      return false;
    }
  }

  if (vid_track) {
    string vp8_id("V_VP8");
    string codec_id(vid_track->GetCodecId());
    if (codec_id != vp8_id) {
      cout << "Video track does not match V_VP8. :" << codec_id << endl;
      return false;
    }
  }

  return true;
}

bool Media::CheckForCues() const {
  assert(segment_.get()!=NULL);

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

void Media::FindCuesChunk(long long start_time_nano,
                          long long end_time_nano,
                          long long& start,
                          long long& end,
                          long long& cue_start_time,
                          long long& cue_end_time) {
  start = 0;
  end = 0;
  cue_start_time = 0;
  cue_end_time = 0;
  assert(segment_.get()!=NULL);
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

    bool b = cues->Find(start_time_nano, track, cue, track_position);
    if (b) {
      long long time_nano;

      while ( (time_nano = cue->GetTime(segment_.get())) < start_time_nano) {
        cue = cues->GetNext(cue);
        if (!cue)
          return; // reached eof
      }

      start = cue->m_element_start;
      cue_start_time = cue->GetTime(segment_.get());

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

      end = cue_prev->m_element_start + cue_prev->m_element_size;
      cue_end_time = cue->GetTime(segment_.get());
    }
  }
}

const mkvparser::AudioTrack* Media::GetAudioTrack() const {
  assert(segment_.get()!=NULL);
  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  assert(tracks!=NULL);

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

int Media::GetAudioChannels() const {
  int channels = 0;
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  if (aud_track) {
    channels = static_cast<int>(aud_track->GetChannels());
  }

  return channels;
}

int Media::GetAudioSampleRate() const {
  int sample_rate = 0;
  const mkvparser::AudioTrack* const aud_track = GetAudioTrack();
  if (aud_track) {
    sample_rate = static_cast<int>(aud_track->GetSamplingRate());
  }

  return sample_rate;
}

long long Media::GetAverageBandwidth() const {
  long long filesize = 0;

  assert(segment_.get()!=NULL);

  // Just estimate for now by parsing through some elements and getting the
  // highest byte value.
  // This needs to change later!!!!
  // paree through the clusters
  const mkvparser::Cluster* cluster = segment_->GetFirst();

  while ((cluster != NULL) && !cluster->EOS()) {
    if ((cluster->m_element_start + cluster->GetElementSize()) > filesize)
      filesize = cluster->m_element_start + cluster->GetElementSize();

    cluster = segment_->GetNext(cluster);
  }

  // check cues element
  const mkvparser::Cues* const cues = segment_->GetCues();
  if (cues && (cues->m_element_start + cues->m_element_size) > filesize)
      filesize = cues->m_element_start + cues->m_element_size;

  // TODO: Add assert that SegmentInfo* is not null.
  long long bandwidth = (filesize * 8) /
    (segment_->GetInfo()->GetDuration() / 1000000000) / 1000;

  return bandwidth;
}

void Media::GetSegmentInfoRange(long long& start, long long& end) const {
  assert(segment_.get()!=NULL);
  const mkvparser::SegmentInfo* const segment_info = segment_->GetInfo();
  assert(segment_info!=NULL);

  start = 0;
  end = 0;  
  if (segment_info) {
    start = segment_info->m_element_start;
    end = segment_info->m_element_start + segment_info->m_element_size;
  }
}

const mkvparser::Track* Media::GetTrack(unsigned int index) const {
  assert(segment_.get()!=NULL);
  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  assert(tracks!=NULL);

  return tracks->GetTrackByIndex(index);
}

void Media::GetTracksRange(long long& start, long long& end) const {
  assert(segment_.get()!=NULL);
  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  assert(tracks!=NULL);

  start = 0;
  end = 0;
  if (tracks) {
    start = tracks->m_element_start;
    end = tracks->m_element_start + tracks->m_element_size;
  }
}

double Media::GetVideoFramerate() const {
  double rate = 0.0;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track) {
    rate = vid_track->GetFrameRate();
  }

  return rate;
}

int Media::GetVideoHeight() const {
  int height = 0;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track) {
    height = static_cast<int>(vid_track->GetHeight());
  }

  return height;
}

int Media::GetVideoWidth() const {
  int width = 0;
  const mkvparser::VideoTrack* const vid_track = GetVideoTrack();
  if (vid_track) {
    width = static_cast<int>(vid_track->GetWidth());
  }

  return width;
}

const mkvparser::VideoTrack* Media::GetVideoTrack() const {
  assert(segment_.get()!=NULL);
  const mkvparser::Tracks* const tracks = segment_->GetTracks();
  assert(tracks!=NULL);

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

void Media::OutputPrototypeManifestCues(std::ostream& o, Indent& indt) {
  assert(segment_.get()!=NULL);

  if (!CheckForCues())
    return;

  indt.Adjust(2);
  o << indt << "<chunkindexlist";
  o << " base_seek_pos=\"" << segment_->m_start << "\" >" << endl;

  const long long duration_nano = GetDurationNanoseconds();
  const long long chunks = (duration_nano / cue_chunk_time_nano_) + 1;
  indt.Adjust(2);

  for (int i=0; i<chunks; ++i) {
    const long long start_time_nano = i*cue_chunk_time_nano_;
    const long long end_time_nano = (i+1)*cue_chunk_time_nano_;

    long long start;
    long long end;
    long long cue_start_nano;
    long long cue_end_nano;
    FindCuesChunk(start_time_nano,
                  end_time_nano,
                  start,
                  end,
                  cue_start_nano,
                  cue_end_nano);
    if ((start == 0) && (end == 0))
      break; // eof

    // Set the end time of the last chunk to the duration of the file
    if (i == chunks-1)
      cue_end_nano = duration_nano;

    o << indt << "<idx start=\"" << cue_start_nano / 1000000000.0 << "\"";
    o << " end=\"" << cue_end_nano / 1000000000.0 << "\"";
    o << " range=\"" << start << "-" << end << "\" />" << endl;
  }

  indt.Adjust(-2);
  o << indt << "</chunkindexlist>" << endl;

  indt.Adjust(-2);
}

std::ostream& operator<< (std::ostream &o, const Media &m) {
  o << "      Media" << endl;
  o << "        id_:" << m.id_ << endl;
  o << "        file_:" << m.file_ << endl;
  o << endl;

  return o;
}

}  // namespace adaptive_manifest