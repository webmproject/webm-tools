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

#include "dash_model.h"
#include "indent.h"
#include "mkvreader.hpp"
#include "webm_file.h"

using indent_webm::Indent;
using std::string;

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
    printf("WebM filename is empty.\n");
    return false;
  }

  webm_file_ = dash_model_.FindWebMFile(webm_filename_);
  if (!webm_file_) {
    printf("Could not find WebM file:%s\n", webm_filename_.c_str());
    return false;
  }

  return true;
}

bool Representation::BitstreamSwitching(
    const Representation& representation) const {
  const WebMFile* const webm = representation.webm_file();
  if (!webm) {
    printf("WebM file is NULL.\n");
    return false;
  }

  if (!webm_file_) {
    printf("WebM file is NULL.\n");
    return false;
  }

  return webm_file_->CheckBitstreamSwitching(*webm);
}

bool Representation::CheckCuesAlignement(
    const Representation& representation) const {
  const WebMFile* const webm = representation.webm_file();
  if (!webm) {
    printf("WebM file is NULL.\n");
    return false;
  }

  if (!webm_file_) {
    printf("WebM file is NULL.\n");
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

bool Representation::OutputDashManifest(FILE* o, Indent* indent) const {
  indent->Adjust(indent_webm::kIncreaseIndent);
  fprintf(o, "%s<Representation id=\"%s\"", indent->indent_str().c_str(),
          id_.c_str());

  const long long prebuffer_ns =
      static_cast<long long>(dash_model_.min_buffer_time() * 1000000000.0);
  fprintf(o, " bandwidth=\"%lld\"",
          webm_file_->PeakBandwidthOverFile(prebuffer_ns));

  // Video
  if (output_video_width_) {
    const int width = webm_file_->GetVideoWidth();
    if (width > 0) {
      fprintf(o, " width=\"%d\"", width);
    }
  }
  if (output_video_height_) {
    const int height = webm_file_->GetVideoHeight();
    if (height > 0) {
      fprintf(o, " height=\"%d\"", height);
    }
  }

  // webm_file_->CalculateVideoFrameRate can take a relatively long time as
  // it must go through every block in the file. For now only output the
  // framerate if the FrameRate element is set in the file. This will most
  // likely need to change later.
  const double rate = webm_file_->GetVideoFramerate();
  if (rate > 0.0) {
    fprintf(o, " framerate=\"%g\"", rate);
  }

  if (output_audio_sample_rate_) {
    const int sample_rate = webm_file_->GetAudioSampleRate();
    if (sample_rate > 0) {
      fprintf(o, " audioSamplingRate=\"%d\"", sample_rate);
    }
  }
  fprintf(o, ">\n");

  indent->Adjust(indent_webm::kIncreaseIndent);
  fprintf(o, "%s<BaseURL>%s</BaseURL>\n", indent->indent_str().c_str(),
          webm_file_->filename().c_str());
  indent->Adjust(indent_webm::kDecreaseIndent);

  const bool b = OutputSegmentBase(o, indent);
  if (!b)
    return false;

  fprintf(o, "%s</Representation>\n", indent->indent_str().c_str());
  indent->Adjust(indent_webm::kDecreaseIndent);

  return true;
}

bool Representation::SubsegmentStartsWithSAP() const {
  if (!webm_file_) {
    printf("WebM file is NULL.\n");
    return false;
  }

  return webm_file_->CuesFirstInCluster();
}

bool Representation::OutputSegmentBase(FILE* o, Indent* indent) const {
  if (!output_header_ && !output_index_)
    return true;
  if (!webm_file_)
    return true;

  indent->Adjust(indent_webm::kIncreaseIndent);
  fprintf(o, "%s<SegmentBase", indent->indent_str().c_str());

  if (output_index_) {
    if (!webm_file_->CheckForCues())
      return false;

    // Output the entire Cues element.
    const mkvparser::Cues* const cues = webm_file_->GetCues();
    const long long start = cues->m_element_start;
    const long long end = start + cues->m_element_size;
    fprintf(o, " indexRange=\"%lld-%lld\"", start, end);
  }

  if (output_header_) {
    fprintf(o, ">\n");

    long long start;
    long long end;
    webm_file_->GetHeaderRange(&start, &end);

    indent->Adjust(indent_webm::kIncreaseIndent);
    fprintf(o, "%s<Initialisation", indent->indent_str().c_str());
    fprintf(o, " range=\"%lld-%lld\" />\n", start, end);
    indent->Adjust(indent_webm::kDecreaseIndent);

    fprintf(o, "%s</SegmentBase>\n", indent->indent_str().c_str());
  } else {
    fprintf(o, " />\n");
  }
  indent->Adjust(indent_webm::kDecreaseIndent);

  return true;
}

}  // namespace webm_dash
