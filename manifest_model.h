/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef MANIFEST_MODEL_H_
#define MANIFEST_MODEL_H_

#include <string>
#include <vector>

namespace adaptive_manifest {

class MediaGroup;
class MediaInterval;

using std::string;
using std::vector;

// This class models how the manifest should be laid out.
class ManifestModel {
 public:
  ManifestModel();
  ~ManifestModel();

  // Inits all of the media groups.
  bool Init();

  // Adds a new MediaGroup that is controlled by the ManifestModel. If
  // sucessful the current MediaGroup will point to the newly added MediaGroup.
  void AddMediaGroup();

  // Adds a new MediaInterval that is controlled by the ManifestModel. If
  // sucessful the current MediaInterval will point to the newly added
  // MediaInterval.
  void AddMediaInterval();

  // Returns the current MediaGroup. If no MediaGroups have been added then
  // returns NULL.
  MediaGroup* CurrentMediaGroup();

  // Returns the current MediaInterval. If no MediaIntervals have been added
  // then returns NULL.
  MediaInterval* CurrentMediaInterval();

  // Search the MediaGroup list for |id|. If not found return NULL.
  //const MediaGroup* FindMediaGroup(const string& id) const;
  MediaGroup* FindMediaGroup(const string& id) const;

  // Write out the manifest file to |output_filename_|.
  bool OutputPrototypeManifestFile();

  bool PrintPrototypeManifestFile();

  const string& output_filename() {return output_filename_;}
  void output_filename(const string& filename) {output_filename_ = filename;}

 private:
  friend std::ostream& operator<< (std::ostream &o, const ManifestModel &m);

  // Maximum duration of all |media_intervals_|.
  double duration_;

  // Media group list for a presentation.
  vector<MediaGroup*> media_groups_;

  // Media interval list for a presentation.
  vector<MediaInterval*> media_intervals_;

  // Path to output the manifest.
  string output_filename_;

  // Disallow copy and assign
  ManifestModel(const ManifestModel&);
  ManifestModel& operator=(const ManifestModel&);
};

}  // namespace adaptive_manifest

#endif  // MANIFEST_MODEL_H_