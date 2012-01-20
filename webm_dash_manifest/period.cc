/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "period.h"

#include <cstdio>

#include "adaptation_set.h"
#include "indent.h"

using indent_webm::Indent;
using std::string;
using std::vector;

namespace webm_dash {

typedef vector<const AdaptationSet*>::const_iterator
    AdaptationSetConstIterator;

Period::Period(const string& id)
    : duration_(0.0),
      id_(id),
      start_(0.0) {
}

Period::~Period() {
}

bool Period::Init() {
  for (AdaptationSetConstIterator iter = adaptation_sets_.begin();
      iter != adaptation_sets_.end();
      ++iter) {
    const double duration = (*iter)->duration();
    if (duration > duration_)
      duration_ = duration;
  }
  return true;
}

void Period::AddAdaptationSetID(const string& id) {
  adaptation_set_ids_.push_back(id);
}

int Period::AdaptationSetIDSize() const {
  return adaptation_set_ids_.size();
}

bool Period::AdaptationSetID(unsigned int index, string* id) const {
  if (!id)
    return false;

  if (index >= adaptation_set_ids_.size())
    return false;

  *id = adaptation_set_ids_.at(index);
  return true;
}

void Period::AddAdaptationSet(const AdaptationSet* as) {
  adaptation_sets_.push_back(as);
}

void Period::OutputDashManifest(FILE* o, Indent* indent) const {
  indent->Adjust(indent_webm::kIncreaseIndent);
  fprintf(o, "%s<Period id=\"%s\"", indent->indent_str().c_str(), id_.c_str());

  fprintf(o, " start=\"PT%gS\"", start_);
  fprintf(o, " duration=\"PT%gS\"", duration_);
  fprintf(o, " >\n");

  for (AdaptationSetConstIterator iter = adaptation_sets_.begin();
      iter != adaptation_sets_.end();
      ++iter) {
    (*iter)->OutputDashManifest(o, indent);
  }

  fprintf(o, "%s</Period>\n", indent->indent_str().c_str());
  indent->Adjust(indent_webm::kDecreaseIndent);
}

}  // namespace adaptive_manifest