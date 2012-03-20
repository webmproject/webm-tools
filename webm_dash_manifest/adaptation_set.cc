/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "adaptation_set.h"

#include <cstdio>
#include <set>

#include "dash_model.h"
#include "indent.h"
#include "representation.h"
#include "webm_file.h"

using std::string;
using std::vector;
using webm_tools::Indent;
using webm_tools::kNanosecondsPerSecond;

namespace webm_dash {

typedef vector<Representation*>::iterator RepresentationIterator;
typedef vector<Representation*>::const_iterator RepresentationConstIterator;

AdaptationSet::AdaptationSet(const string& id, const DashModel& dash)
    : codec_(),
      dash_model_(dash),
      id_(id),
      lang_(),
      mimetype_(),
      profile_(),
      duration_(0.0) {
}

AdaptationSet::~AdaptationSet() {
  for (RepresentationIterator iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    const Representation* const r = *iter;
    delete r;
  }
}

bool AdaptationSet::Init() {
  RepresentationIterator iter;
  for (iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    if ((*iter)->SetWebMFile() == false)
      return false;
  }

  // Check to see if all the codecs match.
  iter = representations_.begin();
  if (!(*iter)->webm_file())
    return false;

  codec_ = (*iter)->webm_file()->GetCodec();
  mimetype_ = (*iter)->webm_file()->GetMimeType();
  for (iter = representations_.begin() + 1;
      iter != representations_.end();
      ++iter) {
    const Representation* const rep = *iter;
    const WebMFile* const webm = rep->webm_file();
    if (!webm)
      return false;
    if (webm->GetCodec() != codec_) {
      fprintf(stderr, "Representation id:%s codec:%s", rep->id().c_str(),
              webm->GetCodec().c_str());
      fprintf(stderr, " does not match in AdaptationSet id:%s\n", id_.c_str());
      return false;
    }
  }

  // Get max duration. All of the streams should have a duration that is close.
  // TODO(fgalligan): Add a check to make sure the durations are close.
  for (iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    const WebMFile* const webm = (*iter)->webm_file();
    if (!webm)
      return false;
    const double duration =
        webm->GetDurationNanoseconds() / kNanosecondsPerSecond;
    if (duration > duration_)
      duration_ = duration;
  }

  // Check that Representation ids do not match. Using set to check if
  // the string has been inserted.
  std::set<string> test_set;
  std::pair<std::set<string>::iterator, bool> ret;
  for (iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    string id = (*iter)->id();
    ret = test_set.insert(id);
    if (ret.second == false) {
      fprintf(stderr,
              "Representation id:%s is duplicate in AdaptationSet id:%s\n",
              id.c_str(), id_.c_str());
      return false;
    }
  }

  const int sample_rate = MatchingAudioSamplingRate();
  const int width = MatchingWidth();
  const int height = MatchingHeight();

  for (iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    if (sample_rate)
      (*iter)->set_output_audio_sample_rate(false);
    if (width)
      (*iter)->set_output_video_width(false);
    if (height)
      (*iter)->set_output_video_height(false);
  }

  return true;
}

void AdaptationSet::AddRepresentation() {
  char temp_str[128];
  sprintf(temp_str, "%d", static_cast<int>(representations_.size()));
  const string id = temp_str;
  representations_.push_back(new Representation(id, dash_model_));
}

Representation* AdaptationSet::CurrentRepresentation() const {
  RepresentationConstIterator iter_curr(representations_.end());
  if (iter_curr == representations_.begin())
    return NULL;

  --iter_curr;
  return *iter_curr;
}

const Representation* AdaptationSet::FindRepresentation(
    const string& id) const {
  for (RepresentationConstIterator iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    if ((*iter)->id() == id)
      return *iter;
  }

  return NULL;
}

void AdaptationSet::OutputDashManifest(FILE* o, Indent* indent) const {
  indent->Adjust(webm_tools::kIncreaseIndent);
  fprintf(o, "%s<AdaptationSet id=\"%s\"", indent->indent_str().c_str(),
          id_.c_str());
  fprintf(o, " mimetype=\"%s\"", mimetype_.c_str());
  fprintf(o, " codecs=\"%s\"", codec_.c_str());

  if (!lang_.empty())
    fprintf(o, " lang=\"%s\"", lang_.c_str());

  const int sample_rate = MatchingAudioSamplingRate();
  if (sample_rate)
    fprintf(o, " audioSamplingRate=\"%d\"", sample_rate);

  const int width = MatchingWidth();
  if (width)
    fprintf(o, " width=\"%d\"", width);

  const int height = MatchingHeight();
  if (height)
    fprintf(o, " height=\"%d\"", height);

  const bool alignment = SubsegmentAlignment();
  if (alignment) {
    fprintf(o, " subsegmentAlignment=\"true\"");
  } else if (representations_.size() > 1 &&
             profile_ == DashModel::webm_on_demand) {
    printf("Warning profile is WebM On-Demand and AdaptationSet id:%s",
           id_.c_str());
    printf(" does not have subSegmentAlignment.\n");
  }

  // WebM is only type '1' or '0'.
  const bool subsegment_starts_with_SAP = SubsegmentStartsWithSAP();
  if (subsegment_starts_with_SAP) {
    fprintf(o, " subsegmentStartsWithSAP=\"1\"");
  } else if (profile_ == DashModel::webm_on_demand) {
    printf("Warning profile is WebM On-Demand and AdaptationSet id:%s",
           id_.c_str());
    printf(" has subsegments that do not start with SAP.\n");
  }

  const bool switching = BitstreamSwitching();
  if (switching)
    fprintf(o, " bitstreamSwitching=\"true\"");
  fprintf(o, ">\n");

  for (RepresentationConstIterator c_iter = representations_.begin();
      c_iter != representations_.end();
      ++c_iter) {
    (*c_iter)->OutputDashManifest(o, indent);
  }

  fprintf(o, "%s</AdaptationSet>\n", indent->indent_str().c_str());
  indent->Adjust(webm_tools::kDecreaseIndent);
}

bool AdaptationSet::BitstreamSwitching() const {
  if (representations_.size() < 2)
    return false;

  RepresentationConstIterator golden_iter(representations_.begin());
  for (RepresentationConstIterator iter = representations_.begin() + 1;
      iter != representations_.end();
      ++iter) {
    const Representation* const r = *iter;
    if (!r->BitstreamSwitching(**golden_iter))
      return false;
  }

  return true;
}

int AdaptationSet::MatchingAudioSamplingRate() const {
  if (representations_.empty())
    return 0;

  const int first_rate = (*representations_.begin())->GetAudioSampleRate();
  if (!first_rate)
      return 0;

  for (RepresentationConstIterator iter = representations_.begin() + 1;
      iter != representations_.end();
      ++iter) {
    if (first_rate != (*iter)->GetAudioSampleRate())
      return 0;
  }

  return first_rate;
}

int AdaptationSet::MatchingHeight() const {
  if (representations_.empty())
    return 0;

  const int first_height = (*representations_.begin())->GetVideoHeight();
  if (!first_height)
      return 0;

  for (RepresentationConstIterator iter = representations_.begin() + 1;
      iter != representations_.end();
      ++iter) {
    if (first_height != (*iter)->GetVideoHeight())
      return 0;
  }

  return first_height;
}

int AdaptationSet::MatchingWidth() const {
  if (representations_.empty())
    return 0;

  const int first_width = (*representations_.begin())->GetVideoWidth();
  if (!first_width)
      return 0;

  for (RepresentationConstIterator iter = representations_.begin() + 1;
      iter != representations_.end();
      ++iter) {
    if (first_width != (*iter)->GetVideoWidth())
      return 0;
  }

  return first_width;
}

bool AdaptationSet::SubsegmentAlignment() const {
 if (representations_.size() < 2)
    return false;

  RepresentationConstIterator golden_iter(representations_.begin());
  for (RepresentationConstIterator iter = representations_.begin() + 1;
      iter != representations_.end();
      ++iter) {
    const Representation* const r = *iter;
    if (!r->CheckCuesAlignement(**golden_iter))
      return false;
  }

  return true;
}

bool AdaptationSet::SubsegmentStartsWithSAP() const {
  if (representations_.empty())
    return false;

  for (RepresentationConstIterator iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    const Representation* const r = *iter;
    if (!r->SubsegmentStartsWithSAP())
      return false;
  }

  return true;
}

}  // namespace webm_dash
