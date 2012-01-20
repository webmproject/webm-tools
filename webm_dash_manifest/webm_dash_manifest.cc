/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "adaptation_set.h"
#include "dash_model.h"
#include "period.h"
#include "representation.h"

using std::cout;
using std::endl;
using std::string;
using std::stringstream;
using webm_dash::DashModel;
using webm_dash::AdaptationSet;
using webm_dash::Period;
using webm_dash::Representation;

static const string VERSION_STRING = "1.0.0.0";

static void Usage() {
  cout << "Usage: webm_dash_manifest <-as [as options]";
  cout << " <-r [r options]>... >... [-o output_file]" << endl;
  cout << endl;
  cout << "Main options:" << endl;
  cout << "-h                    show help" << endl;
  cout << "-v                    show version" << endl;
  cout << "-?                    show help" << endl;
  cout << "-i <string> [...]     Input file list" << endl;
  cout << "[-url <string> [...] ] Base URL list" << endl;
  cout << endl;
  cout << "Period (-p) options:" << endl;
  cout << "-duration <double>    duration in seconds" << endl;
  cout << "-id <string>          id of the Period" << endl;
  cout << "-start <double>       start time in seconds" << endl;
  cout << endl;
  cout << "AdaptationSet (-as) options:" << endl;
  cout << "-id <string>          id of the AdaptationSet" << endl;
  cout << "-lang <string>        lang of the AdaptationSet" << endl;
  cout << endl;
  cout << "Representation (-r) options:" << endl;
  cout << "-id <string>          id of the Media" << endl;
  cout << "-file <string>        filename of the Media" << endl;
  cout << endl;
}

static void ParseAdaptationSetOptions(const string& option_list,
                                      AdaptationSet* as) {
  stringstream ss(option_list);
  string option;
  while(std::getline(ss, option, ',')) {
    stringstream namevalue(option);
    string name;
    string value;
    std::getline(namevalue, name, '=');
    std::getline(namevalue, value, '=');

    if (name == "id") {
      as->set_id(value);
    } else if (name == "lang") {
      as->set_lang(value);
    }
  }
}

static double StringToDouble(const std::string& s) {
   std::istringstream iss(s);
   double value(0.0);
   iss >> value;
   return value;
 }


static void ParsePeriodOptions(const string& option_list, Period* period) {
  stringstream ss(option_list);
  string option;
  while(std::getline(ss, option, ',')) {
    stringstream namevalue(option);
    string name;
    string value;
    std::getline(namevalue, name, '=');
    std::getline(namevalue, value, '=');

    if (name == "id") {
      period->set_id(value);
    } else if (name == "duration") {
      period->set_duration(StringToDouble(value));
    } else if (name == "start") {
      period->set_start(StringToDouble(value));
    }
  }
}

static void ParseRepresentationOptions(const string& option_list,
                                       Representation* r) {
  stringstream ss(option_list);
  string option;
  while(std::getline(ss, option, ',')) {
    stringstream namevalue(option);
    string name;
    string value;
    std::getline(namevalue, name, '=');
    std::getline(namevalue, value, '=');

    if (name == "id") {
      r->set_id(value);
    } else if (name == "file") {
      r->set_webm_filename(value);
    }
  }
}

static bool ParseMainCommandLine(int argc,
                                 char* argv[],
                                 DashModel& model) {
  if (argc < 2)
    return false;

  const int argc_check = argc - 1;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp("-p", argv[i]) && i < argc_check) {
      model.AddPeriod();
      Period* p = model.CurrentPeriod();
      const string option_list(argv[++i]);
      ParsePeriodOptions(option_list, p);
    } else if (!strcmp("-as", argv[i]) && i < argc_check) {
      model.AddAdaptationSet();
      AdaptationSet* as = model.CurrentAdaptationSet();
      const string option_list(argv[++i]);
      ParseAdaptationSetOptions(option_list, as);
    } else if (!strcmp("-r", argv[i]) && i < argc_check) {
      AdaptationSet* as = model.CurrentAdaptationSet();
      if (as) {
        as->AddRepresentation();
        Representation* rep = as->CurrentRepresentation();
        const string option_list(argv[++i]);
        ParseRepresentationOptions(option_list, rep);
      }
    } else if (!strcmp("-o", argv[i]) && i < argc_check) {
      model.set_output_filename(argv[++i]);
    } else if (!strcmp("-v", argv[i])) {
      cout << "version: " << VERSION_STRING << endl;
    } else if ( (!strcmp("-h", argv[i])) || (!strcmp("-?", argv[i])) ) {
      return false;
    } else if (!strcmp("-i", argv[i]) && i < argc_check) {
      model.AppendInputFile(argv[++i]);
    } else if (!strcmp("-url", argv[i]) && i < argc_check) {
      model.AppendBaseUrl(argv[++i]);
    }
  }

  return true;
}

int main(int argc, char* argv[]) {
  DashModel model;

  if (!ParseMainCommandLine(argc, argv, model)) {
    Usage();
    return EXIT_FAILURE;
  }

  if (!model.Init()) {
    cout << "Manifest Model Init() Failed." << endl;
    return EXIT_FAILURE;
  }

  if (!model.OutputDashManifestFile()) {
    cout << "OutputDashManifestFile() Failed." << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
