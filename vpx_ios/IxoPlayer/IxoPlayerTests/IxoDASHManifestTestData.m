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

+ (IxoMutableDASHRepresentation*)repWithID:(NSString*)repID
                                 Bandwidth:(int)bandwidth
                                   BaseURL:(NSString*)baseURL
                     SegmentBaseIndexRange:(NSArray*)indexRange
                       InitializationRange:(NSArray*)initRange {
  IxoMutableDASHRepresentation* rep =
      [[IxoMutableDASHRepresentation alloc] init];
  if (rep) {
    rep.repID = repID;
    rep.bandwidth = bandwidth;
    rep.baseURL = baseURL;
    rep.segmentBaseIndexRange = indexRange;
    rep.initializationRange = initRange;
  }
  return rep;
}

+ (IxoMutableDASHRepresentation*)videoRepWithID:(NSString*)repID
                                      Bandwidth:(int)bandwidth
                                          Width:(int)width
                                         Height:(int)height
                                        BaseURL:(NSString*)baseURL
                          SegmentBaseIndexRange:(NSArray*)indexRange
                            InitializationRange:(NSArray*)initRange {
  IxoMutableDASHRepresentation* rep =
      [[IxoMutableDASHRepresentation alloc] init];
  if (rep) {
    rep.repID = repID;
    rep.bandwidth = bandwidth;
    rep.width = width;
    rep.height = height;
    rep.baseURL = baseURL;
    rep.segmentBaseIndexRange = indexRange;
    rep.initializationRange = initRange;
  }
  return rep;
}

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

  // rep id 4 from adaptation set id 1 in period id 0.
  [audio_as.representations
      addObject:[IxoDASHManifestTestData repWithID:@"4"
                                         Bandwidth:109755
                                           BaseURL:@"glass_171.webm"
                             SegmentBaseIndexRange:@[ @1839501, @1840056 ]
                               InitializationRange:@[ @0, @4700 ]]];

  // rep id 5 from adaptation set id 1 in period id 0.
  [audio_as.representations
      addObject:[IxoDASHManifestTestData repWithID:@"5"
                                         Bandwidth:161358
                                           BaseURL:@"glass_172.webm"
                             SegmentBaseIndexRange:@[ @2698316, @2698871 ]
                               InitializationRange:@[ @0, @4243 ]]];

  // adaptation set id 0 from period id 0 in period id 0.
  IxoMutableDASHAdaptationSet* video_as =
      [[IxoMutableDASHAdaptationSet alloc] init];
  video_as.setID = @"0";
  video_as.codecs = kCodecVP9;
  video_as.mimeType = kMimeTypeWebmVideo;
  video_as.subsegmentAlignment = true;
  video_as.bitstreamSwitching = true;
  video_as.subsegmentStartsWithSAP = 1;

  // rep id 0 from adaptation set id 0 in period id 0.
  [video_as.representations
      addObject:[IxoDASHManifestTestData videoRepWithID:@"0"
                                              Bandwidth:204463
                                                  Width:426
                                                 Height:240
                                                BaseURL:@"glass_242.webm"
                                  SegmentBaseIndexRange:@[ @2983244, @2983799 ]
                                    InitializationRange:@[ @0, @439 ]]];

  // rep id 1 from adaptation set id 0 in period id 0.
  [video_as.representations
      addObject:[IxoDASHManifestTestData videoRepWithID:@"1"
                                              Bandwidth:229655
                                                  Width:640
                                                 Height:360
                                                BaseURL:@"glass_243.webm"
                                  SegmentBaseIndexRange:@[ @3328041, @3328596 ]
                                    InitializationRange:@[ @0, @441 ]]];

  // rep id 2 from adaptation set id 0 in period id 0.
  [video_as.representations
      addObject:[IxoDASHManifestTestData videoRepWithID:@"2"
                                              Bandwidth:279133
                                                  Width:854
                                                 Height:480
                                                BaseURL:@"glass_244.webm"
                                  SegmentBaseIndexRange:@[ @3723746, @3724301 ]
                                    InitializationRange:@[ @0, @441 ]]];

  // rep id 3 from adaptation set id 0 in period id 0.
  [video_as.representations
      addObject:[IxoDASHManifestTestData videoRepWithID:@"3"
                                              Bandwidth:433418
                                                  Width:1280
                                                 Height:720
                                                BaseURL:@"glass_247.webm"
                                  SegmentBaseIndexRange:@[ @4937831, @4938386 ]
                                    InitializationRange:@[ @0, @441 ]]];

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
