// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

// This application uses the webm library from the libwebm project.

// Build instructions for Windows:
1. Install cygwin.
   Note: These instructions were written using cygwin
         x86_64, but should work with the x86 version.
2. Using the cygwin package manager, make sure make and g++
   are installed.
3. Clone libwebm into the same directory as webm-tools. The libwebm and
   webm-tools directories must be siblings in the same root directory.
4. In the libwebm repository, run:
   $ make -f Makefile.unix
5. Back in the webm_crypt source directory, run:
   $ make
6. To confirm webm_crypt works, run:
   $ webm_crypt -test

// Build instructions for Linux:
1. Get the libwebm source code.
   git clone http://git.chromium.org/webm/libwebm.git
2. Build libwebm.
3. Change Makefile to point to your libs and build.
