/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef INDENT_H_
#define INDENT_H_

#include <fstream>

namespace indent_webm {

// Class used to keep track and output spaces to an ostream.
class Indent {
 public:
  explicit Indent(int indent);

  // Changes the number of spaces output. The value adjusted is relative to
  // |indent_|.
  void Adjust(int indent);

 private:
  // Called after |indent_| is changed. This will set |indent_str_| to the
  // proper number of spaces.
  void Update();

  friend std::ostream& operator<< (std::ostream &o, const Indent &indent);

  int indent_;
  std::string indent_str_;

  // Disallow copy and assign
  Indent(const Indent&);
  Indent& operator=(const Indent&);
};

}  // namespace indent_webm

#endif  // PRINT_SEGMENT_H_