/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "representation.h"

#include <cassert>
#include <iostream>

#include "dash_model.h"
#include "indent.h"
#include "mkvreader.hpp"
#include "webm_file.h"

using std::cout;
using std::endl;
using indent_webm::Indent;

namespace webm_dash {

Representation::Representation(const string& id, const DashModel& dash)
    : dash_model_(dash),
      id_(id),
      output_audio_sample_rate_(true),
      output_header_(true),
      output_index_(true),
      output_video_height_(true),
      output_video_width_(true),
      webm_file_(NULL),
      webm_filename_() {
}

Representation::~Representation() {
}

bool Representation::SetWebMFile() {
  if (webm_filename_.empty()) {
    cout << "WebM filename is empty." << endl;
    return false;
  }

  webm_file_ = dash_model_.FindWebMFile(webm_filename_);
  if (!webm_file_) {
    cout << "Could not find WebM file:" << webm_filename_ << endl;
    return false;
  }

  return true;
}

bool Representation::BitstreamSwitching(
    const Representation& representation) const {
  const WebMFile* const webm = representation.webm_file();
  if (!webm) {
    cout << "WebM file is NULL." << endl;
    return false;
  }

  if (!webm_file_) {
    cout << "WebM file is NULL." << endl;
    return false;
  }

  return webm_file_->CheckBitstreamSwitching(*webm);
}

bool Representation::CheckCuesAlignement(
    const Representation& representation) const {
  const WebMFile* const webm = representation.webm_file();
  if (!webm) {
    cout << "WebM file is NULL." << endl;
    return false;
  }

  if (!webm_file_) {
    cout << "WebM file is NULL." << endl;
    return false;
  }

  return webm_file_->CheckCuesAlignement(*webm);
}

int Representation::GetAudioSampleRate() const {
  return webm_file_->GetAudioSampleRate();
}

double Representation::GetVideoFramerate() const {
  return webm_file_->GetVideoFramerate();
}

int Representation::GetVideoHeight() const {
  return webm_file_->GetVideoHeight();
}

int Representation::GetVideoWidth() const {
  return webm_file_->GetVideoWidth();
}

bool Representation::OutputDashManifest(std::ostream& o, Indent& indt) const {
  indt.Adjust(2);
  o << indt << "<Representation id=\"" << id_ << "\"";

  const long long prebuffer_ns =
      static_cast<long long>(dash_model_.min_buffer_time() * 1000000000.0);
  o << " bandwidth=\"" <<
    webm_file_->PeakBandwidthOverFile(prebuffer_ns) << "\"";

  // Video
  if (output_video_width_) {
    const int width = webm_file_->GetVideoWidth();
    if (width > 0) {
      o << " width=\"" << width << "\"";
    }
  }
  if (output_video_height_) {
    const int height = webm_file_->GetVideoHeight();
    if (height > 0) {
      o << " height=\"" << height << "\"";
    }
  }

  // CalculateVideoFrameRate can take a realatively long time as it must go
  // through every block in the file.
  //const double rate = webm_file_->CalculateVideoFrameRate();
  const double rate = webm_file_->GetVideoFramerate();
  if (rate > 0.0) {
    o << " framerate=\"" << rate << "\"";
  }

  if (output_audio_sample_rate_) {
    const int samplerate = webm_file_->GetAudioSampleRate();
    if (samplerate > 0) {
      o << " audioSamplingRate=\"" << samplerate << "\"";
    }
  }

  o << ">" << endl;

  indt.Adjust(2);
  o << indt << "<BaseURL>" << webm_file_->filename() << "</BaseURL>" << endl;
  indt.Adjust(-2);

  const bool b = OutputSegmentBase(o, indt);
  if (!b)
    return false;

  o << indt << "</Representation>" << endl;

  indt.Adjust(-2);

  return true;
}

bool Representation::SubsegmentStartsWithSAP() const {
  if (!webm_file_) {
    cout << "WebM file is NULL." << endl;
    return false;
  }

  return webm_file_->CuesFirstInCluster();
}

bool Representation::OutputSegmentBase(std::ostream& o, Indent& indt) const {
  if (!output_header_ && !output_index_)
    return true;

  assert(webm_file_);

  indt.Adjust(2);
  o << indt << "<SegmentBase";
  if (output_index_) {
    if (!webm_file_->CheckForCues())
      return false;

    // Output the entire Cues element.
    const mkvparser::Cues* const cues = webm_file_->GetCues();
    const long long start = cues->m_element_start;
    const long long end = start + cues->m_element_size;
    o << " indexRange=\"" << start << "-" << end << "\"";
  }

  if (output_header_) {
    o << ">" << endl;

    long long start;
    long long end;
    webm_file_->GetHeaderRange(&start, &end);

    indt.Adjust(2);
    o << indt << "<Initialisation";
    o << " range=\"" << start << "-" << end << "\"" << " />" << endl;
    indt.Adjust(-2);

    o << indt << "</SegmentBase>" << endl;
  } else {
    o << " />" << endl;
  }
  indt.Adjust(-2);

  return true;
}

}  // namespace webm_dash
