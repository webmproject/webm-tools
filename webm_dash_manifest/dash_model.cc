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

#include <cstdio>

#include "adaptation_set.h"
#include "indent.h"
#include "period.h"
#include "webm_file.h"

using std::string;
using std::vector;

namespace webm_dash {

typedef vector<AdaptationSet*>::iterator AdaptationSetIterator;
typedef vector<AdaptationSet*>::const_iterator AdaptationSetConstIterator;
typedef vector<Period*>::iterator PeriodIterator;
typedef vector<Period*>::const_iterator PeriodConstIterator;
typedef vector<WebMFile*>::iterator WebMFileIterator;
typedef vector<WebMFile*>::const_iterator WebMFileConstIterator;

const char DashModel::webm_on_demand[] =
    "urn:webm:dash:profile:webm-on-demand:2012";
const char DashModel::xml_schema_location[] =
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"";
const char DashModel::xml_namespace[] =
    "xmlns=\"urn:mpeg:DASH:schema:MPD:2011\"";
const char DashModel::xml_namespace_location[] =
    "xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011\"";

DashModel::DashModel()
    : type_("static"),
      duration_(0.0),
      min_buffer_time_(1.0),
      profile_(DashModel::webm_on_demand),
      output_filename_("manifest.xml") {
}

DashModel::~DashModel() {
  for (AdaptationSetIterator iter = adaptation_sets_.begin();
      iter != adaptation_sets_.end();
      ++iter) {
    const AdaptationSet* const as = *iter;
    delete as;
  }

  for (PeriodIterator period_iter = periods_.begin();
      period_iter != periods_.end();
      ++period_iter) {
    const Period* const period = *period_iter;
    delete period;
  }

  for (WebMFileIterator webm_iter = webm_files_.begin();
      webm_iter != webm_files_.end();
      ++webm_iter) {
    const WebMFile* const webm = *webm_iter;
    delete webm;
  }
}

bool DashModel::Init() {
  for (WebMFileIterator webm_iter = webm_files_.begin();
      webm_iter != webm_files_.end();
      ++webm_iter) {
    if ((*webm_iter)->Init() == false)
      return false;

    if (profile_ == DashModel::webm_on_demand) {
      if (!(*webm_iter)->OnlyOneStream()) {
        return false;
      }
    }
  }

  AdaptationSetIterator as_iter;
  for (as_iter = adaptation_sets_.begin();
      as_iter != adaptation_sets_.end();
      ++as_iter) {
    (*as_iter)->set_profile(profile_);
    if ((*as_iter)->Init() == false)
      return false;
  }

  // If no periods have been added on the command line add one by default.
  if (periods_.empty())
    AddPeriod();
  Period* period = CurrentPeriod();
  if (!period)
    return false;

  for (as_iter = adaptation_sets_.begin();
      as_iter != adaptation_sets_.end();
      ++as_iter) {
    period->AddAdaptationSetID((*as_iter)->id());
  }

  // Set the media group pointers
  PeriodIterator period_iter;
  for (period_iter = periods_.begin();
      period_iter != periods_.end();
      ++period_iter) {
    period = *period_iter;
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

  for (period_iter = periods_.begin();
      period_iter != periods_.end();
      ++period_iter) {
    if ((*period_iter)->Init() == false)
      return false;
  }

  for (period_iter = periods_.begin();
      period_iter != periods_.end();
      ++period_iter) {
    if (duration_ < (*period_iter)->duration())
      duration_ = (*period_iter)->duration();
  }

  return true;
}

void DashModel::AddAdaptationSet() {
  char temp_str[128];
  sprintf(temp_str, "%d", static_cast<int>(adaptation_sets_.size()));
  const string id = temp_str;

  adaptation_sets_.push_back(new AdaptationSet(id, *this));
}

void DashModel::AppendBaseUrl(const string& url) {
  base_urls_.push_back(url);
}

void DashModel::AppendInputFile(const string& filename) {
  webm_files_.push_back(new WebMFile(filename));
}

void DashModel::AddPeriod()  {
  char temp_str[128];
  sprintf(temp_str, "%d", static_cast<int>(periods_.size()));
  const string id = temp_str;

  periods_.push_back(new Period(id));
}

AdaptationSet* DashModel::CurrentAdaptationSet() const {
  AdaptationSetConstIterator iter_curr(adaptation_sets_.end());
  if (iter_curr == adaptation_sets_.begin())
    return NULL;

  --iter_curr;
  return *iter_curr;
}

Period* DashModel::CurrentPeriod() const {
  PeriodConstIterator iter_curr(periods_.end());
  if (iter_curr == periods_.begin())
    return NULL;

  --iter_curr;
  return *iter_curr;
}

const AdaptationSet* DashModel::FindAdaptationSet(const string& id) const {
  for (AdaptationSetConstIterator iter = adaptation_sets_.begin();
      iter != adaptation_sets_.end();
      ++iter) {
    if ((*iter)->id() == id)
      return *iter;
  }

  return NULL;
}

const WebMFile* DashModel::FindWebMFile(const string& filename) const {
  for (WebMFileConstIterator iter = webm_files_.begin();
      iter != webm_files_.end();
      ++iter) {
    if ((*iter)->filename() == filename)
      return *iter;
  }

  return NULL;
}

bool DashModel::OutputDashManifestFile() const {
  if (output_filename_.empty())
    return false;

  FILE* const o = fopen(output_filename_.c_str(), "w");
  if (!o)
    return false;

  fprintf(o, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(o, "<MPD\n");

  fprintf(o, "  %s\n", xml_schema_location);
  fprintf(o, "  %s\n", xml_namespace);
  fprintf(o, "  %s\n", xml_namespace_location);

  fprintf(o, "  type=\"%s\"\n", type_.c_str());
  fprintf(o, "  mediaPresentationDuration=\"PT%gS\"\n", duration_);
  fprintf(o, "  minBufferTime=\"PT%gS\"\n", min_buffer_time_);
  fprintf(o, "  profiles=\"%s\"", profile_.c_str());

  fprintf(o, ">\n");

  for (vector<string>::const_iterator stri = base_urls_.begin();
      stri != base_urls_.end();
      ++stri) {
    fprintf(o, "  <BaseURL>%s</BaseURL>\n", (*stri).c_str());
  }

  indent_webm::Indent indent(0);
  for (PeriodConstIterator iter = periods_.begin();
      iter != periods_.end();
      ++iter) {
    (*iter)->OutputDashManifest(o, &indent);
  }

  fprintf(o, "</MPD>\n");
  fclose(o);

  return true;
}

}  // namespace webm_dash
