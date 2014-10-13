// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
uniform sampler2D SamplerY;
uniform sampler2D SamplerUV;

varying highp vec2 texCoordVarying;

void main() {
  mediump vec3 yuv;
  lowp vec3 rgb;

  yuv.x = texture2D(SamplerY, texCoordVarying).r;
  yuv.yz = texture2D(SamplerUV, texCoordVarying).rg - vec2(0.5, 0.5);

  // Using BT.709 which is the standard for HDTV
  rgb = mat3(1, 1, 1,
             0, -.18732, 1.8556,
             1.57481, -.46813, 0) * yuv;

  gl_FragColor = vec4(rgb, 1);
}

