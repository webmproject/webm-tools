// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoPlayerTestCommon.h"

#import "IxoDASHManifestParserConstants.h"

const int kNumElementsInRangeArray = 2;

//
// Constants for tests using testdata/manifest_vp9_vorbis.mpd.
//
NSString* const kVP9VorbisDASHMPD1URLString =
    @"http://localhost:8000/manifest_vp9_vorbis.mpd";

// Expected values for data length and MD5 checksum (of the entire MPD file).
const int kVP9VorbisDASHMPD1Length = 2005;
NSString* const kVP9VorbisDASHMPD1MD5 = @"8ab746aecdc021ef92d9162f4ba5dd89";

// Line lengths, their contents, and their offsets (when non-zero) for ranged
// request tests.
const int kVP9VorbisDASHMPD1FirstLineLength = 38;
NSString* const kVP9VorbisDASHMPD1FirstLine =
    @"<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

const int kVP9VorbisDASHMPD1MiddleLineLength = 15;
const int kVP9VorbisDASHMPD1MiddleLineOffset = 1108;
NSString* const kVP9VorbisDASHMPD1MiddleLine = @"<Initialization";

const int kVP9VorbisDASHMPD1LastLineLength = 6;
const int kVP9VorbisDASHMPD1LastLineOffset = 1998;
NSString* const kVP9VorbisDASHMPD1LastLine = @"</MPD>";

@interface IxoDASHManifestTestData : NSObject
+ (NSArray*)buildExpectedDASHManifestDictionaryArray;
@end

@implementation IxoDASHManifestTestData
// Returns an array of dictionaries. Each dictionary contains the expected
// results from a DASH manifest parse attempt.
+ (NSArray*)buildExpectedDASHManifestDictionaryArray {
  // Note: For the sake of brevity not all manifest values are encoded in the
  // dictionaries produced here. Instead, only certain repreesentations from
  // each input manifest are verified. Note that this is only the case when a
  // manifest contains more than one Representation and/or AdaptationSet. When a
  // test manifest contains only one Representation or AdaptationSet it is
  // completely encoded into an NSDictionary.

  NSDictionary* video_rep = [[NSDictionary alloc]
      initWithObjectsAndKeys:
          @"0", @"repID", [NSNumber numberWithInt:204463], @"bandwidth",
          [NSNumber numberWithInt:406], @"width", [NSNumber numberWithInt:240],
          @"height", [NSNumber numberWithInt:0], @"audioSamplingRate",
          [NSNumber numberWithInt:0], @"audioChannelConfig",
          [NSNumber numberWithInt:1], @"startWithSAP", @"glass_242.webm",
          @"baseURL", [NSNull null], @"codecs",
          [NSArray arrayWithObjects:[NSNumber numberWithInt:2983244],
                                    [NSNumber numberWithInt:2983799], nil],
          @"segmentBaseIndexRange",
          [NSArray arrayWithObjects:[NSNumber numberWithInt:0],
                                    [NSNumber numberWithInt:439], nil],
          @"initializationRange", nil];

  NSDictionary* video_as = [[NSDictionary alloc]      initWithObjectsAndKeys:
                            @"0", @"setID",
                            kMimeTypeWebmVideo, @"mimeType",
                            kCodecVP9, @"codecs",
                            nil];

  NSDictionary* manifest = [[NSDictionary alloc]
      initWithObjectsAndKeys:@"PT135.629S", @"mediaPresentationDuration",
                             @"PT1S", @"minBufferTime",
                             [NSNumber numberWithBool:true],
                             @"staticPresentation", nil];

  return [NSArray arrayWithObjects:manifest, nil];
}
@end

// DASH manifest parsing result constants.
const bool kVP9VorbisDASHMPD1IsStatic = true;
NSString* const kVP9VorbisDASHMPD1MediaPresentationDuration = @"PT135.629S";
NSString* const kVP9VorbisDASHMPD1MinBufferTime = @"PT1S";

// Period constants.
NSString* const kVP9VorbisDASHMPD1PeriodID = @"0";
NSString* const kVP9VorbisDASHMPD1PeriodStart = @"PT0S";
NSString* const kVP9VorbisDASHMPD1PeriodDuration = @"PT135.629S";

// Audio AdaptationSet constants.
const int kVP9VorbisDASHMPD1AudioASCount = 1;
NSString* const kVP9VorbisDASHMPD1AudioASID = @"1";
NSString* const kVP9VorbisDASHMPD1AudioASCodecs = @"vorbis";
NSString* const kVP9VorbisDASHMPD1AudioASMimeType = @"audio/webm";
const int kVP9VorbisDASHMPD1AudioASAudioSamplingRate = 44100;

// Video AdaptationSet constants.
const int kVP9VorbisDASHMPD1VideoASCount = 1;
NSString* const kVP9VorbisDASHMPD1VideoASID = @"0";
NSString* const kVP9VorbisDASHMPD1VideoASCodecs = @"vp9";
NSString* const kVP9VorbisDASHMPD1VideoASMimeType = @"video/webm";
const int kVP9VorbisDASHMPD1VideoASWidth = 0;
const int kVP9VorbisDASHMPD1VideoASHeight = 0;

// Audio Representation count and Representation 0 constants.
const int kVP9VorbisDASHMPD1AudioRepCount = 2;
NSString* const kVP9VorbisDASHMPD1AudioRep0ID = @"4";
NSString* const kVP9VorbisDASHMPD1AudioRep0BaseURLString = @"glass_171.webm";
const int kVP9VorbisDASHMPD1AudioRep0IndexRangeStart = 1839501;
const int kVP9VorbisDASHMPD1AudioRep0IndexRangeEnd = 1840056;
const int kVP9VorbisDASHMPD1AudioRep0InitRangeStart = 0;
const int kVP9VorbisDASHMPD1AudioRep0InitRangeEnd = 4700;
const int kVP9VorbisDASHMPD1AudioRep0Bandwidth = 109755;

// Video Representation count and Representation 0 constants.
const int kVP9VorbisDASHMPD1VideoRepCount = 4;
NSString* const kVP9VorbisDASHMPD1VideoRep0ID = @"0";
NSString* const kVP9VorbisDASHMPD1VideoRep0BaseURLString = @"glass_242.webm";
const int kVP9VorbisDASHMPD1VideoRep0IndexRangeStart = 2983244;
const int kVP9VorbisDASHMPD1VideoRep0IndexRangeEnd = 2983799;
const int kVP9VorbisDASHMPD1VideoRep0InitRangeStart = 0;
const int kVP9VorbisDASHMPD1VideoRep0InitRangeEnd = 439;
const int kVP9VorbisDASHMPD1VideoRep0Width = 426;
const int kVP9VorbisDASHMPD1VideoRep0Height = 240;
const int kVP9VorbisDASHMPD1VideoRep0Bandwidth = 204463;

//
// Constants for tests using testdata/manifest_vp8_vorbis.mpd.
//
NSString* const kVP9VorbisDASHMPD2URLString =
    @"http://localhost:8000/manifest_vp8_vorbis.mpd";

// DASH manifest parsing result constants.
const bool kVP9VorbisDASHMPD2IsStatic = true;
NSString* const kVP9VorbisDASHMPD2MediaPresentationDuration = @"PT280.199S";
NSString* const kVP9VorbisDASHMPD2MinBufferTime = @"PT1S";

// Period constants.
NSString* const kVP9VorbisDASHMPD2PeriodID = @"0";
NSString* const kVP9VorbisDASHMPD2PeriodStart = @"PT0S";
NSString* const kVP9VorbisDASHMPD2PeriodDuration = @"PT280.199S";

// Audio AdaptationSet constants.
const int kVP9VorbisDASHMPD2AudioASCount = 1;
NSString* const kVP9VorbisDASHMPD2AudioASID = @"1";
NSString* const kVP9VorbisDASHMPD2AudioASCodecs = @"vorbis";
NSString* const kVP9VorbisDASHMPD2AudioASMimeType = @"audio/webm";
const int kVP9VorbisDASHMPD2AudioASAudioSamplingRate = 48000;

// Video AdaptationSet constants.
const int kVP9VorbisDASHMPD2VideoASCount = 1;
NSString* const kVP9VorbisDASHMPD2VideoASID = @"";
NSString* const kVP9VorbisDASHMPD2VideoASCodecs = @"";
NSString* const kVP9VorbisDASHMPD2VideoASMimeType = @"";

// Audio Representation count and Representation 0 constants.
const int kVP9VorbisDASHMPD2AudioRepCount;
NSString* const kVP9VorbisDASHMPD2AudioRep0ID = @"";
NSString* const kVP9VorbisDASHMPD2AudioRep0BaseURLString = @"";
const int kVP9VorbisDASHMPD2AudioRep0IndexRangeStart;
const int kVP9VorbisDASHMPD2AudioRep0IndexRangeEnd;
const int kVP9VorbisDASHMPD2AudioRep0InitRangeStart;
const int kVP9VorbisDASHMPD2AudioRep0InitRangeEnd;
const int kVP9VorbisDASHMPD2AudioRep0Bandwidth;

// Video Representation count and Representation 2 constants.
const int kVP9VorbisDASHMPD2VideoRepCount;
NSString* const kVP9VorbisDASHMPD2VideoRep2ID = @"";
NSString* const kVP9VorbisDASHMPD2VideoRep2BaseURLString = @"";
const int kVP9VorbisDASHMPD2VideoRep2IndexRangeStart;
const int kVP9VorbisDASHMPD2VideoRep2IndexRangeEnd;
const int kVP9VorbisDASHMPD2VideoRep2InitRangeStart;
const int kVP9VorbisDASHMPD2VideoRep2InitRangeEnd;
const int kVP9VorbisDASHMPD2VideoRep2Width;
const int kVP9VorbisDASHMPD2VideoRep2Height;
const int kVP9VorbisDASHMPD2VideoRep2Bandwidth;
