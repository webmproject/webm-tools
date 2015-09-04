// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoDASHManifestTestData.h"

#import "IxoDASHManifestParserConstants.h"
#import "IxoPlayerTestCommon.h"

@interface IxoDASHManifestTestData ()
+ (IxoMutableDASHManifest*)getVP9VorbisDASHMPD1Manifest;
@end

@implementation IxoDASHManifestTestData

// Returns expected manifest resulting from parse of manifest at
// |kVP9VorbisDASHMPD1URLString| (testdata/manifest_vp9_vorbis.mpd).
+ (IxoMutableDASHManifest*)getVP9VorbisDASHMPD1Manifest {
  // adaptation set id 1 from period id 0.
  IxoMutableDASHAdaptationSet* audio_as =
      [[IxoMutableDASHAdaptationSet alloc] init];
  audio_as.setID = @"1";
  audio_as.codecs = kCodecVorbis;
  audio_as.mimeType = kMimeTypeWebmAudio;
  audio_as.audioSamplingRate = 44100;
  audio_as.subsegmentAlignment = true;
  audio_as.subsegmentStartsWithSAP = 1;

  IxoMutableDASHRepresentation* audio_rep =
      [[IxoMutableDASHRepresentation alloc] init];

  // rep id 4 from adaptation set id 1 in period id 0.
  audio_rep.repID = @"4";
  audio_rep.bandwidth = 109755;
  audio_rep.baseURL = @"glass_171.webm";
  audio_rep.segmentBaseIndexRange = @[ @1839501, @1840056 ];
  audio_rep.initializationRange = @[ @0, @4700 ];
  [audio_as.representations addObject:[audio_rep copy]];

  // rep id 5 from adaptation set id 1 in period id 0.
  audio_rep.repID = @"5";
  audio_rep.bandwidth = 161358;
  audio_rep.baseURL = @"glass_172.webm";
  audio_rep.segmentBaseIndexRange = @[ @2698316, @2698871 ];
  audio_rep.initializationRange = @[ @0, @4243 ];
  [audio_as.representations addObject:audio_rep];

  // adaptation set id 0 from period id 0 in period id 0.
  IxoMutableDASHAdaptationSet* video_as =
      [[IxoMutableDASHAdaptationSet alloc] init];
  video_as.setID = @"0";
  video_as.codecs = kCodecVP9;
  video_as.mimeType = kMimeTypeWebmVideo;
  video_as.subsegmentAlignment = true;
  video_as.bitstreamSwitching = true;
  video_as.subsegmentStartsWithSAP = 1;

  IxoMutableDASHRepresentation* video_rep =
      [[IxoMutableDASHRepresentation alloc] init];

  // rep id 0 from adaptation set id 0 in period id 0.
  video_rep.repID = @"0";
  video_rep.bandwidth = 204463;
  video_rep.width = 426;
  video_rep.height = 240;
  video_rep.baseURL = @"glass_242.webm";
  video_rep.segmentBaseIndexRange = @[ @2983244, @2983799 ];
  video_rep.initializationRange = @[ @0, @439 ];
  [video_as.representations addObject:[video_rep copy]];

  // rep id 1 from adaptation set id 0 in period id 0.
  video_rep.repID = @"1";
  video_rep.bandwidth = 229655;
  video_rep.width = 640;
  video_rep.height = 360;
  video_rep.baseURL = @"glass_243.webm";
  video_rep.segmentBaseIndexRange = @[ @3328041, @3328596 ];
  video_rep.initializationRange = @[ @0, @441 ];
  [video_as.representations addObject:[video_rep copy]];

  // rep id 2 from adaptation set id 0 in period id 0.
  video_rep.repID = @"2";
  video_rep.bandwidth = 279133;
  video_rep.width = 854;
  video_rep.height = 480;
  video_rep.baseURL = @"glass_244.webm";
  video_rep.segmentBaseIndexRange = @[ @3723746, @3724301 ];
  video_rep.initializationRange = @[ @0, @441 ];
  [video_as.representations addObject:[video_rep copy]];

  // rep id 3 from adaptation set id 0 in period id 0.
  video_rep.repID = @"3";
  video_rep.bandwidth = 433418;
  video_rep.width = 1280;
  video_rep.height = 720;
  video_rep.baseURL = @"glass_247.webm";
  video_rep.segmentBaseIndexRange = @[ @4937831, @4938386 ];
  video_rep.initializationRange = @[ @0, @441 ];
  [video_as.representations addObject:video_rep];

  // period id 0.
  IxoMutableDASHPeriod* period = [[IxoMutableDASHPeriod alloc] init];
  period.periodID = @"0";
  period.start = @"PT0S";
  period.duration = @"PT135.629S";
  period.audioAdaptationSets =
      [[NSMutableArray alloc] initWithObjects:audio_as, nil];
  period.videoAdaptationSets =
      [[NSMutableArray alloc] initWithObjects:video_as, nil];

  IxoMutableDASHManifest* manifest = [[IxoMutableDASHManifest alloc] init];
  manifest.mediaPresentationDuration = @"PT135.629S";
  manifest.minBufferTime = @"PT1S";
  manifest.staticPresentation = true;
  manifest.period = period;

  return manifest;
}

+ (IxoMutableDASHManifest*)getExpectedManifestForURLString:(NSString*)string {
  IxoMutableDASHManifest* manifest = nil;
  if ([string isEqualToString:kVP9VorbisDASHMPD1URLString]) {
    manifest = [IxoDASHManifestTestData getVP9VorbisDASHMPD1Manifest];
  } /*else if ([string isEqualToString:kVP9VorbisDASHMPD2URLString]) {
  } else if ([string isEqualToString:kVP9VorbisDASHMPD3URLString]) {
  }*/
  return manifest;
}
@end
