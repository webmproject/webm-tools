// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
attribute vec4 position;
attribute vec2 texCoord;

varying vec2 texCoordVarying;

void main() {
  gl_Position = position;
  texCoordVarying = texCoord;
}

