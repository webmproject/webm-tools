// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

// This application uses the crypto library from the Chromium project.

// Build instructions for Windows:
1. Follow http://www.chromium.org/developers/how-tos/build-instructions-windows
   until the Building Chromium step to get the crypto lib source code.
2. Build the chromium/src/base/base.sln, chromium/src/crypto/crypto.sln, and
   chromium/src/base/third_party/dynamic_annotations.sln solutions.
3. Get the libwebm source code.
   git clone http://git.chromium.org/webm/libwebm.git
4. Build libwebm.
5. Open environment.vsprops file and change CHROMIUM_INC's value to your
   Chromium's src directory.

// Build instructions for Linux:
1. Follow http://code.google.com/p/chromium/wiki/LinuxBuildInstructions to
   build chrome application.
2. Get the libwebm source code.
   git clone http://git.chromium.org/webm/libwebm.git
3. Build libwebm.
4. Change Makefile to point to your libs and build.

Known Issues:

- Windows crypto does not support CTR.
  http://code.google.com/p/webm/issues/detail?id=423

- Linux does not work with SymmetricKey::Import.
  http://code.google.com/p/webm/issues/detail?id=422
