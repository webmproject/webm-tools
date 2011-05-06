/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef MEDIA_GROUP_H_
#define MEDIA_GROUP_H_

#include <string>
#include <vector>

namespace indent_webm {
class Indent;
}
namespace adaptive_manifest {

class Media;

using std::string;
using std::vector;

class MediaGroup {
 public:
  explicit MediaGroup(const string& id);
  ~MediaGroup();

  // Inits all of the Media within this media group. Checks to make sure all
  // of the codecs in |media_| match. Calculates max duration.
  bool Init();

  // Adds a new Media that is controlled by the MediaGroup. If sucessful the
  // current Media will point to the newly added Media.
  void AddMedia();

  // Returns the current Media. If no Media has been added then returns NULL.
  Media* CurrentMedia();

  // Search the Media list for |id|. If not found return NULL
  const Media* FindMedia(const string& id) const;

  // Outputs MediaGroup in the prototype format.
  void OutputPrototypeManifest(std::ostream& o, indent_webm::Indent& indt);

  const double& duration() {return duration_;}

  const string& id() {return id_;}
  void id(const string& id) {id_ = id;}

  const string& lang() {return lang_;}
  void lang(const string& lang) {lang_ = lang;}

 private:
  friend std::ostream& operator<< (std::ostream &o, const MediaGroup &mg);

  // Codec string of all |media_|.
  string codec_;

  // Used to differentiate between different media groups within a
  // presentation.
  string id_;

  // Languge attribute of a media group.
  string lang_;

  // Maximum duration of all |media_|.
  double duration_;

  // Media list for this media group.
  vector<Media*> media_;

  // Disallow copy and assign
  MediaGroup(const MediaGroup&);
  MediaGroup& operator=(const MediaGroup&);
};

}  // namespace adaptive_manifest

#endif  // MEDIA_GROUP_H_