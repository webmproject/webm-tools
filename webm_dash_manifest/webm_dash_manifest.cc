/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "adaptation_set.h"
#include "dash_model.h"
#include "period.h"
#include "representation.h"

using std::string;
using webm_dash::DashModel;
using webm_dash::AdaptationSet;
using webm_dash::Period;
using webm_dash::Representation;

static const string VERSION_STRING = "1.0.0.0";

static void Usage() {
  printf("Usage: webm_dash_manifest [-o output_file] [-p options] ");
  printf("<-as [as options] <-r [r options]>... >...\n");
  printf("\n");
  printf("Main options:\n");
  printf("-h | -?               show help\n");
  printf("-v                    show version\n");
  printf("-url <string> [...]   Base URL list\n");
  printf("-profile <string>     Set profile.\n");
  printf("\n");
  printf("Period (-p) options:\n");
  printf("-duration <double>    duration in seconds\n");
  printf("-id <string>          id of the Period\n");
  printf("-start <double>       start time in seconds\n");
  printf("\n");
  printf("AdaptationSet (-as) options:\n");
  printf("-id <string>          id of the AdaptationSet\n");
  printf("-lang <string>        lang of the AdaptationSet\n");
  printf("\n");
  printf("Representation (-r) options:\n");
  printf("-id <string>          id of the Media\n");
  printf("-file <string>        Input file\n");
  printf("\n");
}

static bool ParseOption(const string& option, string* name, string* value) {
  if (!name || !value)
    return false;

  const size_t equal_sign = option.find_first_of('=');
  if (equal_sign == string::npos)
    return false;

  *name = option.substr(0, equal_sign);
  *value = option.substr(equal_sign + 1, string::npos);
  return true;
}

static void ParseAdaptationSetOptions(const string& option_list,
                                      AdaptationSet* as) {
  size_t start = 0;
  size_t end = option_list.find_first_of(',');
  for (;;) {
    const string option = option_list.substr(start, end);
    string name;
    string value;
    if (ParseOption(option, &name, &value)) {
      if (name == "id") {
        as->set_id(value);
      } else if (name == "lang") {
        as->set_lang(value);
      }
    }

    if (end == string::npos)
      break;
    start = end + 1;
    end = option_list.find_first_of(',', start);
  }
}

static double StringToDouble(const std::string& s) {
  return strtod(s.c_str(), NULL);
}

static void ParsePeriodOptions(const string& option_list, Period* period) {
  size_t start = 0;
  size_t end = option_list.find_first_of(',');
  for (;;) {
    const string option = option_list.substr(start, end);
    string name;
    string value;
    if (ParseOption(option, &name, &value)) {
      if (name == "id") {
        period->set_id(value);
      } else if (name == "duration") {
        period->set_duration(StringToDouble(value));
      } else if (name == "start") {
        period->set_start(StringToDouble(value));
      }
    }

    if (end == string::npos)
      break;
    start = end + 1;
    end = option_list.find_first_of(',', start);
  }
}

static void ParseRepresentationOptions(const string& option_list,
                                       DashModel* model,
                                       Representation* r) {
  size_t start = 0;
  size_t end = option_list.find_first_of(',');
  for (;;) {
    const string option = option_list.substr(start, end);
    string name;
    string value;
    if (ParseOption(option, &name, &value)) {
      if (name == "id") {
        r->set_id(value);
      } else if (name == "file") {
        model->AppendInputFile(value);
        r->set_webm_filename(value);
      }
    }

    if (end == string::npos)
      break;
    start = end + 1;
    end = option_list.find_first_of(',', start);
  }
}

static bool ParseMainCommandLine(int argc,
                                 char* argv[],
                                 DashModel* model) {
  if (argc < 2) {
    Usage();
    return false;
  }

  if (!model) {
    fprintf(stderr, "model parameter is NULL.\n");
    return EXIT_FAILURE;
  }

  const int argc_check = argc - 1;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp("-p", argv[i]) && i < argc_check) {
      model->AddPeriod();
      Period* const p = model->CurrentPeriod();
      const string option_list(argv[++i]);
      ParsePeriodOptions(option_list, p);
    } else if (!strcmp("-as", argv[i]) && i < argc_check) {
      model->AddAdaptationSet();
      AdaptationSet* const as = model->CurrentAdaptationSet();
      const string option_list(argv[++i]);
      ParseAdaptationSetOptions(option_list, as);
    } else if (!strcmp("-r", argv[i]) && i < argc_check) {
      AdaptationSet* const as = model->CurrentAdaptationSet();
      if (as) {
        as->AddRepresentation();
        Representation* const rep = as->CurrentRepresentation();
        const string option_list(argv[++i]);
        ParseRepresentationOptions(option_list, model, rep);
      }
    } else if (!strcmp("-o", argv[i]) && i < argc_check) {
      model->set_output_filename(argv[++i]);
    } else if (!strcmp("-v", argv[i])) {
      printf("version: %s\n", VERSION_STRING.c_str());
    } else if ( (!strcmp("-h", argv[i])) || (!strcmp("-?", argv[i])) ) {
      Usage();
      return false;
    } else if (!strcmp("-url", argv[i]) && i < argc_check) {
      model->AppendBaseUrl(argv[++i]);
    } else if (!strcmp("-profile", argv[i]) && i < argc_check) {
      model->set_profile(argv[++i]);
    }
  }

  return true;
}

int main(int argc, char* argv[]) {
  DashModel model;

  if (!ParseMainCommandLine(argc, argv, &model)) {
    return EXIT_FAILURE;
  }

  if (!model.Init()) {
    fprintf(stderr, "Manifest Model Init() Failed.\n");
    return EXIT_FAILURE;
  }

  if (!model.OutputDashManifestFile()) {
    fprintf(stderr, "OutputDashManifestFile() Failed.\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
