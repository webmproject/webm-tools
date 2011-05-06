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

using std::endl;

namespace adaptive_manifest {

ManifestModel::ManifestModel()
    :  output_filename_("manifest.xml") {
}

ManifestModel::~ManifestModel() {
  vector<MediaGroup*>::iterator iter;
  for( iter = media_groups_.begin(); iter != media_groups_.end(); ++iter ) {
    MediaGroup* mg = *iter;
    delete mg; 
  }
}

bool ManifestModel::Init() {
  vector<MediaGroup*>::iterator iter;
  for( iter = media_groups_.begin(); iter != media_groups_.end(); ++iter ) {
    if ((*iter)->Init() == false)
      return false;
  }

  return true;
}

void ManifestModel::AddMediaGroup() {
  std::ostringstream temp;
  temp << media_groups_.size();
  const string id = temp.str();

  media_groups_.push_back(new MediaGroup(id));
}

MediaGroup* ManifestModel::CurrentMediaGroup() {
  vector<MediaGroup*>::iterator iter_curr(media_groups_.end());

  if (iter_curr == media_groups_.begin())
    return NULL;

  --iter_curr;
  return *iter_curr;

  return NULL;
}

const MediaGroup* ManifestModel::FindMediaGroup(const string& id) const {
  vector<MediaGroup*>::const_iterator iter;
  for( iter = media_groups_.begin(); iter != media_groups_.end(); ++iter ) {
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
  of << "<presentation>" << endl;

  indent_webm::Indent indt(0);
  vector<MediaGroup*>::const_iterator iter;
  for( iter = media_groups_.begin(); iter != media_groups_.end(); ++iter ) {
    (*iter)->OutputPrototypeManifest(of, indt);
  }

  of << "</presentation>" << endl;

  of.close();
  return true;
}

bool ManifestModel::PrintPrototypeManifestFile() {
  assert(output_filename_.c_str()!=NULL);

  std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
  std::cout << "<presentation>" << endl;

  indent_webm::Indent indt(0);
  vector<MediaGroup*>::const_iterator iter;
  for( iter = media_groups_.begin(); iter != media_groups_.end(); ++iter ) {
    (*iter)->OutputPrototypeManifest(std::cout, indt);
  }

  std::cout << "</presentation>" << endl;

  return true;
}

std::ostream& operator<< (std::ostream &o, const ManifestModel &m)
{
  o << "ManifestModel" << endl;
  vector<MediaGroup*>::const_iterator iter;
  for( iter = m.media_groups_.begin();
       iter != m.media_groups_.end();
       ++iter ) {
    MediaGroup* mg = *iter;
    o << *mg;
  }
	return o ;
}

}  // namespace adaptive_manifest