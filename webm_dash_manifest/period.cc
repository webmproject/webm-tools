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

#include <sstream>

#include "adaptation_set.h"
#include "indent.h"

using std::endl;
using indent_webm::Indent;

namespace webm_dash {

Period::Period(const string& id)
    : duration_(0.0),
      id_(id),
      start_(0.0) {
}

Period::~Period() {
}

bool Period::Init() {
  vector<const AdaptationSet*>::const_iterator iter;
  for(iter = adaptation_sets_.begin();
      iter != adaptation_sets_.end();
      ++iter) {
    double duration = (*iter)->duration();
    if (duration > duration_)
      duration_ = duration;
  }
  return true;
}

void Period::AddAdaptationSetID(const string& id) {
  adaptation_set_ids_.push_back(id);
}

int Period::AdaptationSetIDSize() const{
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

void Period::OutputDashManifest(std::ostream& o, Indent& indt) const {
  indt.Adjust(2);
  o << indt << "<Period id=\"" << id_ << "\"";
  o << " start=\"PT" << start_ << "S\"";
  o << " duration=\"PT" << duration_ << "S\"";
  o << " >" << endl;

  vector<const AdaptationSet*>::const_iterator iter;
  for(iter = adaptation_sets_.begin(); iter != adaptation_sets_.end(); ++iter) {
    (*iter)->OutputDashManifest(o, indt);
  }

  o << indt << "</Period>" << endl;

  indt.Adjust(-2);
}

}  // namespace adaptive_manifest