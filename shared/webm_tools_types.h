// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef SHARED_WEBM_TOOL_TYPES_H_
#define SHARED_WEBM_TOOL_TYPES_H_

// Copied from Chromium basictypes.h
// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define WEBM_TOOLS_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

namespace webm_tools {

typedef unsigned char      uint8;
typedef int                int32;
typedef unsigned int       uint32;
typedef long long          int64;
typedef unsigned long long uint64;

}  //end namespace webm_tools

#endif // SHARED_WEBM_TOOL_TYPES_H_
