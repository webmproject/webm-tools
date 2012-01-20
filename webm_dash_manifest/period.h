/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBM_DASH_MANIFEST_PERIOD_H_
#define WEBM_DASH_MANIFEST_PERIOD_H_

#include <string>
#include <vector>

namespace indent_webm {
class Indent;
}
namespace webm_dash {

class AdaptationSet;

using std::string;
using std::vector;

class Period {
 public:
  explicit Period(const string& id);
  ~Period();

  // Inits all of the Media within this media group. Checks to make sure all
  // of the codecs in |media_| match. Calculates max duration.
  bool Init();

  // Add the AdaptationSet ids that will be contained within the Period.
  void AddAdaptationSetID(const string& id);

  // Returns the number of AdaptationSet ids.
  int AdaptationSetIDSize() const;

  // Returns AdaptationSet |id| referenced by |index|. Returns true if |id| is
  // assigned.
  bool AdaptationSetID(unsigned int index, string* id) const;

  // Adds a AdaptationSet that is controlled by another object.
  void AddAdaptationSet(const AdaptationSet* as);

  // Outputs AdaptationSet in the prototype format.
  void OutputDashManifest(FILE* o, indent_webm::Indent* indent) const;

  const double& duration() const { return duration_; }
  void set_duration(const double& duration) { duration_ = duration; }

  const string& id() const { return id_; }
  void set_id(const string& id) { id_ = id; }

  const double& start() const { return start_; }
  void set_start(const double& start) { start_ = start; }

 private:
  // Maximum duration of all |adaptation_sets_|.
  double duration_;

  // Used to differentiate between different Period within a MPD.
  string id_;

  // Start time in seconds for all of the AdaptationSets within this Period.
  double start_;

  // List of AdaptationSets for this Period.
  vector<const AdaptationSet*> adaptation_sets_;

  vector<string> adaptation_set_ids_;

  // Disallow copy and assign
  Period(const Period&);
  Period& operator=(const Period&);
};

}  // namespace webm_dash

#endif  // WEBM_DASH_MANIFEST_PERIOD_H_