/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBM_DASH_MANIFEST_INDENT_H_
#define WEBM_DASH_MANIFEST_INDENT_H_

#include <string>

#include "webm_tools_types.h"

namespace indent_webm {

const int kIncreaseIndent = 2;
const int kDecreaseIndent = -2;

// Class used to keep track and output spaces to an ostream.
class Indent {
 public:
  explicit Indent(int indent);

  // Changes the number of spaces output. The value adjusted is relative to
  // |indent_|.
  void Adjust(int indent);

  std::string indent_str() const { return indent_str_; }

 private:
  // Called after |indent_| is changed. This will set |indent_str_| to the
  // proper number of spaces.
  void Update();

  int indent_;
  std::string indent_str_;

  WEBM_TOOLS_DISALLOW_COPY_AND_ASSIGN(Indent);
};

}  // namespace indent_webm

#endif  // WEBM_DASH_MANIFEST_INDENT_H_