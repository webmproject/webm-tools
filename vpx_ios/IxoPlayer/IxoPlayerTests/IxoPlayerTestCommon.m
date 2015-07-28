// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoPlayerTestCommon.h"

NSString* const kStaticManifestURLString =
    @"http://localhost:8000/vp9_glass/manifest_vp9_vorbis.mpd";

/*
const char* const kStaticManifestURLString =
    "http://localhost:8000/vp9_glass/manifest_vp9_vorbis.mpd";

@implementation IxoPlayerTestCommon

// This method should never be called, but is valid. It's here to quiet the
// warning about |kStaticManifestURLString| going unused here.
- (id)init {
  (void)kStaticManifestURLString;
  self = [super init];
  return self;
}

+ (NSString*)StringFromCString:(const char*)cString {
  return [NSString stringWithCString:cString encoding:NSUTF8StringEncoding];
}

+ (NSURL*)URLFromCString:(const char*)cString {
  return [NSURL URLWithString:[IxoPlayerTestCommon StringFromCString:cString]];
}

@end  // @implementation IxoPlayerTestCommon
*/