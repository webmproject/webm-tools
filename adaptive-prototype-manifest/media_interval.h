/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef MEDIA_INTERVAL_H_
#define MEDIA_INTERVAL_H_

#include <string>
#include <vector>

namespace indent_webm {
class Indent;
}
namespace adaptive_manifest {

class MediaGroup;

using std::string;
using std::vector;

class MediaInterval {
 public:
  explicit MediaInterval(const string& id);
  ~MediaInterval();

  // Inits all of the Media within this media group. Checks to make sure all
  // of the codecs in |media_| match. Calculates max duration.
  bool Init();

  // Add the media group ids that will be contained within the media interval.
  void AddMediaGroupID(const string& id);

  // Returns the number of media group ids.
  int MediaGroupIDSize();

  // Returns media group |id| referenced by |index|. Returns true if |id| is
  // assigned.
  bool MediaGroupID(unsigned int index, string& id);

  // Adds a MediaGroup that is controlled by another object.
  void AddMediaGroup(MediaGroup* mg);

  // Outputs MediaGroup in the prototype format.
  void OutputPrototypeManifest(std::ostream& o, indent_webm::Indent& indt);

  const double& duration() {return duration_;}
  void duration(const double& duration) {duration_ = duration;}

  const string& id() {return id_;}
  void id(const string& id) {id_ = id;}

  const double& start() {return start_;}
  void start(const double& start) {start_ = start;}

 private:
  friend std::ostream& operator<< (std::ostream &o, const MediaInterval &mi);

  // Maximum duration of all |media_groups_|.
  double duration_;

  // Used to differentiate between different media intervals within a
  // presentation.
  string id_;

  // Start time in seconds for all of the media groups within this interval.
  double start_;

  // Media list for this media group.
  vector<MediaGroup*> media_groups_;

  vector<string> media_group_ids_;

  // Disallow copy and assign
  MediaInterval(const MediaInterval&);
  MediaInterval& operator=(const MediaInterval&);
};

}  // namespace adaptive_manifest

#endif  // MEDIA_INTERVAL_H_