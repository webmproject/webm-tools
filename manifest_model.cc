/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "manifest_model.h"

#include <assert.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include "indent.h"
#include "media_group.h"
#include "media_interval.h"

using std::endl;

namespace adaptive_manifest {

ManifestModel::ManifestModel()
    : duration_(0.0),
      output_filename_("manifest.xml"),
      manifest_version_(1) {
}

ManifestModel::~ManifestModel() {
  vector<MediaGroup*>::iterator iter;
  for(iter = media_groups_.begin(); iter != media_groups_.end(); ++iter) {
    MediaGroup* mg = *iter;
    delete mg;
  }

  vector<MediaInterval*>::iterator mi_iter;
  for(mi_iter = media_intervals_.begin();
      mi_iter != media_intervals_.end();
      ++mi_iter) {
    MediaInterval* mi = *mi_iter;
    delete mi;
  }
}

bool ManifestModel::Init() {
  vector<MediaGroup*>::iterator mg_iter;
  for(mg_iter = media_groups_.begin();
      mg_iter != media_groups_.end();
      ++mg_iter) {
    if ((*mg_iter)->Init() == false)
      return false;
  }

  // If no media intervals has been added on the command line add one by
  // default.
  if (media_intervals_.empty()) {
    AddMediaInterval();
    MediaInterval* mi = CurrentMediaInterval();

    for(mg_iter = media_groups_.begin();
        mg_iter != media_groups_.end();
        ++mg_iter) {
      mi->AddMediaGroupID((*mg_iter)->id());
    }
  }

  // Set the media group pointers
  vector<MediaInterval*>::iterator mi_iter;
  for(mi_iter = media_intervals_.begin();
      mi_iter != media_intervals_.end();
      ++mi_iter) {
    MediaInterval* mi = *mi_iter;

    for (int i=0; i<mi->MediaGroupIDSize(); ++i) {
      string id;
      if (!mi->MediaGroupID(i, id))
        return false;

      MediaGroup* mg = FindMediaGroup(id);
      if (!mg)
        return false;

      mi->AddMediaGroup(mg);
    }
  }

  for(mi_iter = media_intervals_.begin();
      mi_iter != media_intervals_.end();
      ++mi_iter) {
    if ((*mi_iter)->Init() == false)
      return false;
  }

  for(mi_iter = media_intervals_.begin();
      mi_iter != media_intervals_.end();
      ++mi_iter) {
    if (duration_ < (*mi_iter)->duration())
      duration_ = (*mi_iter)->duration();
  }

  return true;
}

void ManifestModel::AddMediaGroup() {
  std::ostringstream temp;
  temp << media_groups_.size();
  const string id = temp.str();

  media_groups_.push_back(new MediaGroup(id));
}

void ManifestModel::AddMediaInterval()  {
  std::ostringstream temp;
  temp << media_intervals_.size();
  const string id = temp.str();

  media_intervals_.push_back(new MediaInterval(id));
}

MediaGroup* ManifestModel::CurrentMediaGroup() {
  vector<MediaGroup*>::iterator iter_curr(media_groups_.end());

  if (iter_curr == media_groups_.begin())
    return NULL;

  --iter_curr;
  return *iter_curr;

  return NULL;
}

MediaInterval* ManifestModel::CurrentMediaInterval() {
  vector<MediaInterval*>::iterator iter_curr(media_intervals_.end());

  if (iter_curr == media_intervals_.begin())
    return NULL;

  --iter_curr;
  return *iter_curr;

  return NULL;
}

MediaGroup* ManifestModel::FindMediaGroup(const string& id) const {
  vector<MediaGroup*>::const_iterator iter;
  for(iter = media_groups_.begin(); iter != media_groups_.end(); ++iter) {
    if ((*iter)->id() == id)
      return *iter;
  }

  return NULL;
}

bool ManifestModel::OutputPrototypeManifestFile() {
  assert(output_filename_.c_str()!=NULL);
  std::ofstream of(output_filename_.c_str());
  if (!of)
    return false;

  of << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
  of << "<Presentation";
  of << " duration=\"" << duration_ << "\"";
  of << " version=\"" << manifest_version_ << "\"";
  of << " >" << endl;

  indent_webm::Indent indt(0);
  vector<MediaInterval*>::const_iterator iter;
  for(iter = media_intervals_.begin(); iter != media_intervals_.end(); ++iter) {
    (*iter)->OutputPrototypeManifest(of, indt);
  }

  of << "</Presentation>" << endl;

  of.close();
  return true;
}

bool ManifestModel::PrintPrototypeManifestFile() {
  assert(output_filename_.c_str()!=NULL);

  std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
  std::cout << "<presentation>" << endl;

  indent_webm::Indent indt(0);
  vector<MediaInterval*>::const_iterator iter;
  for(iter = media_intervals_.begin(); iter != media_intervals_.end(); ++iter) {
    (*iter)->OutputPrototypeManifest(std::cout, indt);
  }

  std::cout << "</presentation>" << endl;

  return true;
}

std::ostream& operator<< (std::ostream &o, const ManifestModel &m)
{
  o << "ManifestModel" << endl;
  vector<MediaInterval*>::const_iterator iter;
  for(iter = m.media_intervals_.begin();
      iter != m.media_intervals_.end();
      ++iter ) {
    MediaInterval* mi = *iter;
    o << *mi;
  }
  return o ;
}

}  // namespace adaptive_manifest