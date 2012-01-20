/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBM_DASH_MANIFEST_DASH_MODEL_H_
#define WEBM_DASH_MANIFEST_DASH_MODEL_H_

#include <string>
#include <vector>

namespace webm_dash {

class AdaptationSet;
class Period;
class WebMFile;

// This class models how the manifest should be laid out.
class DashModel {
 public:
  DashModel();
  ~DashModel();

  // Inits all of the media groups.
  bool Init();

  // Adds a new AdaptationSet that is controlled by the DashModel. If
  // successful the current AdaptationSet will point to the newly added
  // AdaptationSet.
  void AddAdaptationSet();

  // Adds another base url to the manifest at the end of the list.
  void AppendBaseUrl(const std::string& url);

  // Adds another input file at the end of the list.
  void AppendInputFile(const std::string& filename);

  // Adds a new Period that is controlled by the DashModel. If
  // successful the current Period will point to the newly added
  // Period.
  void AddPeriod();

  // Returns the current AdaptationSet. If no AdaptationSets have been added
  // then returns NULL.
  AdaptationSet* CurrentAdaptationSet();

  // Returns the current Period. If no Periods have been added
  // then returns NULL.
  Period* CurrentPeriod();

  // Search the AdaptationSet list for |id|. If not found return NULL.
  const AdaptationSet* FindAdaptationSet(const std::string& id) const;

  // Search the webm file list for |filename|. If not found return NULL.
  const WebMFile* FindWebMFile(const std::string& filename) const;

  // Write out the manifest file to |output_filename_|.
  bool OutputDashManifestFile() const;

  double min_buffer_time() const { return min_buffer_time_; }

  const std::string& output_filename() const { return output_filename_; }
  void set_output_filename(const std::string& file) { output_filename_ = file; }

 private:
  // XML Schema location.
  static const char xml_schema_location[];

  // Dash namespace.
  static const char xml_namespace[];

  // Dash namespace location.
  static const char xml_namespace_location[];

  // File type. Either static or live.
  std::string type_;

  // Maximum duration of all |periods_|.
  double duration_;

  // Minimum buffer time in seconds.
  double min_buffer_time_;

  // WebM Dash profile.
  const std::string profile_;

  // List of base urls for the mpd.
  std::vector<std::string> base_urls_;

  // List of input WebM files.
  std::vector<WebMFile*> webm_files_;

  // Adaptation set list for a presentation.
  std::vector<AdaptationSet*> adaptation_sets_;

  // Period list for a presentation.
  std::vector<Period*> periods_;

  // Path to output the manifest.
  std::string output_filename_;

  // Disallow copy and assign
  DashModel(const DashModel&);
  DashModel& operator=(const DashModel&);
};

}  // namespace webm_dash

#endif  // WEBM_DASH_MANIFEST_DASH_MODEL_H_