/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "media_interval.h"

#include <sstream>

#include "indent.h"
#include "media_group.h"

using std::endl;
using indent_webm::Indent;

namespace adaptive_manifest {

MediaInterval::MediaInterval(const string& id)
    : duration_(0.0), 
      id_(id),
      start_(0.0) {
}

MediaInterval::~MediaInterval() {
}

bool MediaInterval::Init() {
  vector<MediaGroup*>::const_iterator iter;
  for(iter = media_groups_.begin(); iter != media_groups_.end(); ++iter) {
    double duration = (*iter)->duration();
    if (duration > duration_)
      duration_ = duration;
  }
  return true;
}

void MediaInterval::AddMediaGroupID(const string& id) {
  media_group_ids_.push_back(id);
}

int MediaInterval::MediaGroupIDSize() {
  return media_group_ids_.size();
}

bool MediaInterval::MediaGroupID(unsigned int index, string& id) {
  if (index >= media_group_ids_.size())
    return false;

  id = media_group_ids_.at(index);
  return true;
}

void MediaInterval::AddMediaGroup(MediaGroup* mg) {
  media_groups_.push_back(mg);
}

void MediaInterval::OutputPrototypeManifest(std::ostream& o, Indent& indt) {
  indt.Adjust(2);
  o << indt << "<MediaInterval id=\"" << id_ << "\"";
  o << " start=\"" << start_ << "\"";
  o << " duration=\"" << duration_ << "\"";
  o << " >" << endl;

  vector<MediaGroup*>::const_iterator iter;
  for(iter = media_groups_.begin(); iter != media_groups_.end(); ++iter) {
    (*iter)->OutputPrototypeManifest(o, indt);
  }

  o << indt << "</MediaInterval>" << endl;

  indt.Adjust(-2);
}

std::ostream& operator<< (std::ostream &o, const MediaInterval &mi) {
  o << "  MediaInterval" << endl;
  o << "    id_:" << mi.id_ << endl;
  o << "    start_:" << mi.start_ << endl;
  o << "    duration_:" << mi.duration_ << endl;

  vector<MediaGroup*>::const_iterator iter;
  for(iter = mi.media_groups_.begin(); iter != mi.media_groups_.end(); ++iter) {
    MediaGroup* mg = *iter;
    o << *mg;
  }

	return o;
}

}  // namespace adaptive_manifest