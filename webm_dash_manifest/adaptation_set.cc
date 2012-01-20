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

#include <cassert>
#include <set>
#include <sstream>

#include "dash_model.h"
#include "indent.h"
#include "representation.h"
#include "webm_file.h"

using indent_webm::Indent;

namespace webm_dash {

AdaptationSet::AdaptationSet(const string& id, const DashModel& dash)
    : codec_(),
      dash_model_(dash),
      id_(id),
      lang_(),
      mimetype_(),
      duration_(0.0) {
}

AdaptationSet::~AdaptationSet() {
  vector<Representation*>::iterator iter;
  for(iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    const Representation* const r = *iter;
    delete r;
  }
}

bool AdaptationSet::Init() {
  vector<Representation*>::iterator iter;
  for(iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    if ((*iter)->SetWebMFile() == false)
      return false;
  }

  // Check to see if all the codecs match.
  iter = representations_.begin();
  assert((*iter)->webm_file());
  codec_ = (*iter)->webm_file()->GetCodec();
  mimetype_ = (*iter)->webm_file()->GetMimeType();
  for(iter = representations_.begin() + 1;
      iter != representations_.end();
      ++iter) {
    const Representation* const rep = *iter;
    const WebMFile* const webm = rep->webm_file();
    assert(webm);
    if (webm->GetCodec() != codec_) {
      printf("Representation id:%s  codec:%s ", rep->id().c_str(),
             webm->GetCodec().c_str());
      printf("does not match in AdaptationSet id:%s\n", id_.c_str());
      return false;
    }
  }

  // Get max duration. All of the streams should have a duration that is close.
  // TODO(fgalligan): Add a check to make sure the durations are close.
  for(iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    const WebMFile* const webm = (*iter)->webm_file();
    assert(webm);
    const double duration = webm->GetDurationNanoseconds() / 1000000000.0;
    if (duration > duration_)
      duration_ = duration;
  }

  // Check that Representation ids do not match. Using set to check if
  // the string has been inserted.
  std::set<string> test_set;
  std::pair<std::set<string>::iterator, bool> ret;
  for(iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    string id = (*iter)->id();
    ret = test_set.insert(id);
    if (ret.second == false) {
      printf("Representation id:%s is duplicate in AdaptationSet id:%s\n",
             id.c_str(), id_.c_str());
      return false;
    }
  }

  const int sample_rate = MatchingAudioSamplingRate();
  const int width = MatchingWidth();
  const int height = MatchingHeight();

  for(iter = representations_.begin();
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
  std::ostringstream temp;
  temp << representations_.size();
  const string id = temp.str();
  representations_.push_back(new Representation(id, dash_model_));
}

Representation* AdaptationSet::CurrentRepresentation() {
  vector<Representation*>::iterator iter_curr(representations_.end());
  if (iter_curr == representations_.begin())
    return NULL;

  --iter_curr;
  return *iter_curr;
}

const Representation* AdaptationSet::FindRepresentation(
    const string& id) const {
  vector<Representation*>::const_iterator iter;
  for(iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    if ((*iter)->id() == id)
      return *iter;
  }

  return NULL;
}

void AdaptationSet::OutputDashManifest(FILE* o, Indent* indent) const {
  indent->Adjust(2);
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
  if (alignment)
    fprintf(o, " subsegmentAlignment=\"true\"");

  // WebM is only type '1' or '0'.
  const bool subsegment_starts_with_SAP = SubsegmentStartsWithSAP();
  if (subsegment_starts_with_SAP)
    fprintf(o, " subsegmentStartsWithSAP=\"1\"");

  const bool switching = BitstreamSwitching();
  if (switching)
    fprintf(o, " bitstreamSwitching=\"true\"");
  fprintf(o, ">\n");

  vector<Representation*>::const_iterator c_iter;
  for(c_iter = representations_.begin();
      c_iter != representations_.end();
      ++c_iter) {
    (*c_iter)->OutputDashManifest(o, indent);
  }

  fprintf(o, "%s</AdaptationSet>\n", indent->indent_str().c_str());
  indent->Adjust(-2);
}

bool AdaptationSet::BitstreamSwitching() const {
  if (representations_.size() < 2)
    return false;

  vector<Representation*>::const_iterator golden_iter(
      representations_.begin());
  vector<Representation*>::const_iterator iter;

  for(iter = representations_.begin() + 1;
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

  vector<Representation*>::const_iterator iter;
  for(iter = representations_.begin() + 1;
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

  vector<Representation*>::const_iterator iter;
  for(iter = representations_.begin() + 1;
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

  vector<Representation*>::const_iterator iter;
  for(iter = representations_.begin() + 1;
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

  vector<Representation*>::const_iterator golden_iter(
      representations_.begin());
  vector<Representation*>::const_iterator iter;

  for(iter = representations_.begin() + 1;
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

  vector<Representation*>::const_iterator iter;
  for(iter = representations_.begin();
      iter != representations_.end();
      ++iter) {
    const Representation* const r = *iter;
    if (!r->SubsegmentStartsWithSAP())
      return false;
  }

  return true;
}

}  // namespace webm_dash
