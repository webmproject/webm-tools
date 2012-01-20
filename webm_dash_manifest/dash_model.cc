/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "dash_model.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

#include "adaptation_set.h"
#include "indent.h"
#include "period.h"
#include "webm_file.h"

using std::endl;

namespace webm_dash {

DashModel::DashModel()
    : xml_schema_location_("xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""),
      xml_namespace_("xmlns=\"urn:mpeg:DASH:schema:MPD:2011\""),
      xml_namespace_location_("xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011\""),
      type_("static"),
      duration_(0.0),
      min_buffer_time_(1.0),
      profile_("urn:webm:dash:profile:webm-on-demand:2012"),
      pretty_format_(true),
      output_filename_("manifest.xml") {
}

DashModel::~DashModel() {
  vector<AdaptationSet*>::iterator iter;
  for(iter = adaptation_sets_.begin();
      iter != adaptation_sets_.end();
      ++iter) {
    const AdaptationSet* const as = *iter;
    delete as;
  }

  vector<Period*>::iterator period_iter;
  for(period_iter = periods_.begin();
      period_iter != periods_.end();
      ++period_iter) {
    const Period* const period = *period_iter;
    delete period;
  }

  vector<WebMFile*>::iterator webm_iter;
  for(webm_iter = webm_files_.begin();
      webm_iter != webm_files_.end();
      ++webm_iter) {
    const WebMFile* const webm = *webm_iter;
    delete webm;
  }
}

bool DashModel::Init() {
  vector<WebMFile*>::iterator webm_iter;
  for(webm_iter = webm_files_.begin();
      webm_iter != webm_files_.end();
      ++webm_iter) {
    if ((*webm_iter)->Init() == false)
      return false;

    if (!(*webm_iter)->OnlyOneStream()) {
      return false;
    }
  }

  vector<AdaptationSet*>::iterator as_iter;
  for(as_iter = adaptation_sets_.begin();
      as_iter != adaptation_sets_.end();
      ++as_iter) {
    if ((*as_iter)->Init() == false)
      return false;
  }

  // If no periods have been added on the command line add one by default.
  if (periods_.empty())
    AddPeriod();
  Period* period = CurrentPeriod();
  if (!period)
    return false;

  for(as_iter = adaptation_sets_.begin();
      as_iter != adaptation_sets_.end();
      ++as_iter) {
    period->AddAdaptationSetID((*as_iter)->id());
  }

  // Set the media group pointers
  vector<Period*>::iterator period_iter;
  for(period_iter = periods_.begin();
      period_iter != periods_.end();
      ++period_iter) {
    Period* period = *period_iter;

    for (int i = 0; i < period->AdaptationSetIDSize(); ++i) {
      string id;
      if (!period->AdaptationSetID(i, &id))
        return false;

      const AdaptationSet* const as = FindAdaptationSet(id);
      if (!as)
        return false;

      period->AddAdaptationSet(as);
    }
  }

  for(period_iter = periods_.begin();
      period_iter != periods_.end();
      ++period_iter) {
    if ((*period_iter)->Init() == false)
      return false;
  }

  for(period_iter = periods_.begin();
      period_iter != periods_.end();
      ++period_iter) {
    if (duration_ < (*period_iter)->duration())
      duration_ = (*period_iter)->duration();
  }

  return true;
}

void DashModel::AddAdaptationSet() {
  std::ostringstream temp;
  temp << adaptation_sets_.size();
  const string id = temp.str();

  adaptation_sets_.push_back(new AdaptationSet(id, *this));
}

void DashModel::AppendBaseUrl(const string& url) {
  base_urls_.push_back(url);
}

void DashModel::AppendInputFile(const string& filename) {
  webm_files_.push_back(new WebMFile(filename));
}

void DashModel::AddPeriod()  {
  std::ostringstream temp;
  temp << periods_.size();
  const string id = temp.str();

  periods_.push_back(new Period(id));
}

AdaptationSet* DashModel::CurrentAdaptationSet() {
  vector<AdaptationSet*>::iterator iter_curr(adaptation_sets_.end());
  if (iter_curr == adaptation_sets_.begin())
    return NULL;

  --iter_curr;
  return *iter_curr;
}

Period* DashModel::CurrentPeriod() {
  vector<Period*>::iterator iter_curr(periods_.end());
  if (iter_curr == periods_.begin())
    return NULL;

  --iter_curr;
  return *iter_curr;
}

const AdaptationSet* DashModel::FindAdaptationSet(const string& id) const {
  vector<AdaptationSet*>::const_iterator iter;
  for(iter = adaptation_sets_.begin();
      iter != adaptation_sets_.end();
      ++iter) {
    if ((*iter)->id() == id)
      return *iter;
  }

  return NULL;
}

const WebMFile* DashModel::FindWebMFile(const string& filename) const {
  vector<WebMFile*>::const_iterator iter;
  for(iter = webm_files_.begin(); iter != webm_files_.end(); ++iter) {
    if ((*iter)->filename() == filename)
      return *iter;
  }

  return NULL;
}

bool DashModel::OutputDashManifestFile() const {
  assert(output_filename_.c_str());
  std::ofstream of(output_filename_.c_str());
  if (!of)
    return false;

  of << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
  of << "<MPD";

  indent_webm::Indent indt(2);

  if (pretty_format_) {
    of << endl;
    of << indt << xml_schema_location_ << endl;
    of << indt << xml_namespace_ << endl;
    of << indt << xml_namespace_location_ << endl;
    of << indt << "type=\"" << type_ << "\"" << endl;
    of << indt << "mediaPresentationDuration=\"PT" << duration_ << "S\""
       << endl;
    of << indt << "minBufferTime=\"PT" << min_buffer_time_ << "S\"" << endl;
    of << indt << "profiles=\"" << profile_ << "\"";
  } else {
    of << " " << xml_schema_location_;
    of << " " << xml_namespace_;
    of << " " << xml_namespace_location_;
    of << " " << "type=\"" << type_ << "\"";
    of << " " << "mediaPresentationDuration=\"PT" << duration_ << "S\"";
    of << " " << "minBufferTime=\"PT" << min_buffer_time_ << "S\"";
    of << " " << "profiles=\"" << profile_ << "\"";
  }

  of << ">" << endl;

  vector<string>::const_iterator stri;
  for(stri = base_urls_.begin(); stri != base_urls_.end(); ++stri) {
    of << indt << "<BaseURL>" << *stri << "</BaseURL>" << endl;
  }

  indt.Adjust(-2);

  vector<Period*>::const_iterator iter;
  for(iter = periods_.begin(); iter != periods_.end(); ++iter) {
    (*iter)->OutputDashManifest(of, indt);
  }

  of << "</MPD>" << endl;

  of.close();
  return true;
}

}  // namespace adaptive_manifest