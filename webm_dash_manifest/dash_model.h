/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef DASH_MODEL_H_
#define DASH_MODEL_H_

#include <string>
#include <vector>

namespace webm_dash {

class AdaptationSet;
class Period;
class WebMFile;

using std::string;
using std::vector;

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
  void AppendBaseUrl(const string& url);

  // Adds another input file at the end of the list.
  void AppendInputFile(const string& filename);

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
  const AdaptationSet* FindAdaptationSet(const string& id) const;

  // Search the webm file list for |filename|. If not found return NULL.
  const WebMFile* FindWebMFile(const string& filename) const;

  // Write out the manifest file to |output_filename_|.
  bool OutputDashManifestFile() const;

  double min_buffer_time() const { return min_buffer_time_; }

  const string& output_filename() const { return output_filename_; }
  void set_output_filename(const string& file) { output_filename_ = file; }

 private:
  // XML Schema location.
  string xml_schema_location_;

  // Dash namespace.
  string xml_namespace_;

  // Dash namespace location.
  string xml_namespace_location_;

  // File type. Either static or live.
  string type_;

  // Maximum duration of all |periods_|.
  double duration_;

  // Minimum buffer time in seconds.
  double min_buffer_time_;

  // WebM Dash profile.
  string profile_;

  // If true try and make the output more readable.
  bool pretty_format_;

  // List of base urls for the mpd.
  vector<string> base_urls_;

  // List of input WebM files.
  vector<WebMFile*> webm_files_;

  // Adaptation set list for a presentation.
  vector<AdaptationSet*> adaptation_sets_;

  // Period list for a presentation.
  vector<Period*> periods_;

  // Path to output the manifest.
  string output_filename_;

  // Disallow copy and assign
  DashModel(const DashModel&);
  DashModel& operator=(const DashModel&);
};

}  // namespace webm_dash

#endif  // DASH_MODEL_H_