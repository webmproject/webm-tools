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

#include "webm_tools_types.h"

namespace webm_tools {
class Indent;
}  // namespace webm_tools

namespace webm_dash {

class AdaptationSet;

class Period {
 public:
  explicit Period(const std::string& id);
  ~Period();

  // Calculates max duration.
  bool Init();

  // Add the AdaptationSet ids that will be contained within the Period.
  void AddAdaptationSetID(const std::string& id);

  // Returns the number of AdaptationSet ids.
  int AdaptationSetIDSize() const;

  // Returns AdaptationSet |id| referenced by |index|. Returns true if |id| is
  // assigned.
  bool AdaptationSetID(unsigned int index, std::string* id) const;

  // Adds a AdaptationSet that is controlled by another object.
  void AddAdaptationSet(const AdaptationSet* as);

  // Outputs AdaptationSet in the prototype format.
  void OutputDashManifest(FILE* o, webm_tools::Indent* indent) const;

  double duration() const { return duration_; }
  void set_duration(double duration) { duration_ = duration; }

  std::string id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  double start() const { return start_; }
  void set_start(double start) { start_ = start; }

 private:
  // Maximum duration of all |adaptation_sets_|.
  double duration_;

  // Used to differentiate between different Period within a MPD.
  std::string id_;

  // Start time in seconds for all of the AdaptationSets within this Period.
  double start_;

  // List of AdaptationSets for this Period.
  std::vector<const AdaptationSet*> adaptation_sets_;

  std::vector<std::string> adaptation_set_ids_;

  WEBM_TOOLS_DISALLOW_COPY_AND_ASSIGN(Period);
};

}  // namespace webm_dash

#endif  // WEBM_DASH_MANIFEST_PERIOD_H_
