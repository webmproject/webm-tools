/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBM_DASH_MANIFEST_ADAPTATION_SET_H_
#define WEBM_DASH_MANIFEST_ADAPTATION_SET_H_

#include <string>
#include <vector>

namespace indent_webm {
class Indent;
}

namespace webm_dash {

class DashModel;
class Representation;

class AdaptationSet {
 public:
  AdaptationSet(const std::string& id, const DashModel& dash);
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
  const Representation* FindRepresentation(const std::string& id) const;

  // Outputs AdaptationSet in the WebM Dash format.
  void OutputDashManifest(FILE* o, indent_webm::Indent* indent) const;

  const double& duration() const { return duration_; }

  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  const std::string& lang() const { return lang_; }
  void set_lang(const std::string& lang) { lang_ = lang; }

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
  std::string codec_;

  // The main class for the manifest.
  const DashModel& dash_model_;

  // Used to differentiate between different media groups within a
  // presentation.
  std::string id_;

  // Languge attribute.
  std::string lang_;

  // Mimetype attribute.
  std::string mimetype_;

  // Maximum duration of all |media_|.
  double duration_;

  // TODO(fgalligan): Think about changing representations_ to a map with rep
  // id as the key.
  // Media list for this media group.
  std::vector<Representation*> representations_;

  // Disallow copy and assign
  AdaptationSet(const AdaptationSet&);
  AdaptationSet& operator=(const AdaptationSet&);
};

}  // namespace webm_dash

#endif  // WEBM_DASH_MANIFEST_ADAPTATION_SET_H_