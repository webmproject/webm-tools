/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ADAPTATION_SET_H_
#define ADAPTATION_SET_H_

#include <string>
#include <vector>

namespace indent_webm {
class Indent;
}

namespace webm_dash {

class DashModel;
class Representation;

using std::string;
using std::vector;

class AdaptationSet {
 public:
  explicit AdaptationSet(const string& id, const DashModel& dash);
  ~AdaptationSet();

  // Inits all of the files within this adaptation set. Checks to make sure all
  // of the codecs in the files match. Calculates max duration.
  bool Init();

  // Adds a new Representation that is controlled by the AdaptationSet. If
  // successful the current Representation will point to the newly added
  // Representation.
  void AddRepresentation();

  // Returns the current Representation. If no Representation has been added
  // then returns NULL.
  Representation* CurrentRepresentation();

  // Search the Representation list for |id|. If not found return NULL
  const Representation* FindRepresentation(const string& id) const;

  // Outputs AdaptationSet in the WebM Dash format.
  void OutputDashManifest(std::ostream& o, indent_webm::Indent& indt) const;

  const double& duration() const {return duration_;}

  const string& id() const {return id_;}
  void set_id(const string& id) {id_ = id;}

  const string& lang() const {return lang_;}
  void set_lang(const string& lang) {lang_ = lang;}

 private:
  // Check all the files within the AdaptationSet to see if they conform to
  // the bitstreamSwitching attribute.
  bool BitstreamSwitching() const;

  // Check all the Representations within the AdaptationSet to see if they
  // have the same sample rate. Returns the sample rate if all the
  // Representations have a matching sample rate. Returns 0 if the sample
  // rates do not match or the Representations do not contain audio.
  int MatchingAudioSamplingRate() const;

  // Check all the Representations within the AdaptationSet to see if they
  // have the same height. Returns the height if all the Representations have
  // a matching height. Returns 0 if the heights do not match or the
  // Representations do not contain video.
  int MatchingHeight() const;

  // Check all the Representations within the AdaptationSet to see if they
  // have the same width. Returns the width if all the Representations have
  // a matching width. Returns 0 if the widths do not match or the
  // Representations do not contain video.
  int MatchingWidth() const;

  // Check all the files within the AdaptationSet to see if they conform to
  // the subsegmentAlignment attribute.
  bool SubsegmentAlignment() const;

  // Check all the files within the AdaptationSet to see if they conform to
  // the subsegmentStartsWithSAP attribute.
  bool SubsegmentStartsWithSAP() const;

  // Codec string of all the files.
  string codec_;

  // The main class for the manifest.
  const DashModel& dash_model_;

  // Used to differentiate between different media groups within a
  // presentation.
  string id_;

  // Languge attribute.
  string lang_;

  // Mimetype attribute.
  string mimetype_;

  // Maximum duration of all |media_|.
  double duration_;

  // Media list for this media group.
  vector<Representation*> representations_;

  // Disallow copy and assign
  AdaptationSet(const AdaptationSet&);
  AdaptationSet& operator=(const AdaptationSet&);
};

}  // namespace webm_dash

#endif  // ADAPTATION_SET_H_