/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "media_group.h"

#include <sstream>

#include "indent.h"
#include "media.h"

using std::endl;
using indent_webm::Indent;

namespace adaptive_manifest {

MediaGroup::MediaGroup(const string& id)
    : codec_(),
      id_(id),
      lang_(),
      duration_(0.0) {
}

MediaGroup::~MediaGroup() {
  vector<Media*>::iterator iter;
  for( iter = media_.begin(); iter != media_.end(); ++iter ) {
    Media* m = *iter;
    delete m; 
  }
}

bool MediaGroup::Init() {
  vector<Media*>::iterator iter;
  for( iter = media_.begin(); iter != media_.end(); ++iter ) {
    if ((*iter)->Init() == false)
      return false;
  }

  // Check to see if all the codecs match.
  iter = media_.begin();
  codec_ = (*iter)->GetCodec();
  for(iter = media_.begin()+1; iter != media_.end(); ++iter ) {
    if ((*iter)->GetCodec() != codec_)
      return false;
  }

  // Get max duration. All of the streams should have a duration that is close.
  // TODO: Add a check to make sure the durations are close.
  for( iter = media_.begin(); iter != media_.end(); ++iter ) {
    double duration = (*iter)->GetDurationNanoseconds() / 1000000000.0;
    if (duration > duration_)
      duration_ = duration;
  }

  return true;
}

void MediaGroup::AddMedia() {
  std::ostringstream temp;
  temp << media_.size();
  const string id = temp.str();

  media_.push_back(new Media(id));
}

Media* MediaGroup::CurrentMedia() {
  vector<Media*>::iterator iter_curr(media_.end());

  if (iter_curr == media_.begin())
    return NULL;

  --iter_curr;
  return *iter_curr;

  return NULL;
}

const Media* MediaGroup::FindMedia(const string& id) const {
  vector<Media*>::const_iterator iter;
  for( iter = media_.begin(); iter != media_.end(); ++iter ) {
    if ((*iter)->id() == id)
      return *iter;
  }

  return NULL;
}

void MediaGroup::OutputPrototypeManifest(std::ostream& o, Indent& indt) {
  indt.Adjust(2);
  o << indt << "<mediagroup id=\"" << id_ << "\"";
  o << " type=\"video/webm\" codecs=\"" << codec_ << "\"";

  if (!lang_.empty()) {
    o << " lang=\"" << lang_ << "\"";
  }

  o << " alignment=\"false\"";
  o << " duration=\"" << duration_ << "\" >" << endl;

  vector<Media*>::const_iterator iter;
  for( iter = media_.begin(); iter != media_.end(); ++iter ) {
    (*iter)->OutputPrototypeManifest(o, indt);
  }

  o << indt << "</mediagroup>" << endl;

  indt.Adjust(-2);
}

std::ostream& operator<< (std::ostream &o, const MediaGroup &mg)
{
  o << "  MediaGroup" << endl;
  o << "    id_:" << mg.id_ << endl;
  o << "    lang_:" << mg.lang_ << endl;

  vector<Media*>::const_iterator iter;
  for( iter = mg.media_.begin(); iter != mg.media_.end(); ++iter ) {
    Media* m = *iter;
    o << *m;
  }

	return o;
}

}  // namespace adaptive_manifest