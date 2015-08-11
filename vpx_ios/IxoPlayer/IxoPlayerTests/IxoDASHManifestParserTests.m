// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoDASHManifestParser.h"

#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <XCTest/XCTest.h>

#import "IxoPlayerTestCommon.h"

@interface IxoDASHManifestParserTests : XCTestCase
@end

@implementation IxoDASHManifestParserTests {
  NSArray* allManifests;
  NSDictionary* expectedManifest;
}

// Note: -setUp and -tearDown are called for each test.




- (void)setUp {

  //
  expectedManifest = [[NSDictionary alloc]
      initWithObjectsAndKeys:@"PT135.629S", @"mediaPresentationDuration",
                             @"PT1S", @"minBufferTime",
                             [NSNumber numberWithBool:true],
                             @"staticPresentation",
                             nil];
  [super setUp];
}

- (void)tearDown {
  [super tearDown];
}

- (void)testParseDASH1 {
  NSURL* const manifest_url = [NSURL URLWithString:kVP9VorbisDASHMPD1URLString];
  IxoDASHManifestParser* parser =
      [[IxoDASHManifestParser alloc] initWithManifestURL:manifest_url];
  const bool parse_ok = [parser parse];
  XCTAssertTrue(parse_ok);
  IxoDASHManifest* const manifest = parser.manifest;
  NSArray* const keys = expectedManifest.allKeys;

  for (NSString* key in keys) {
    id expected_value = [expectedManifest objectForKey:key];
    id actual_value = [manifest valueForKey:key];
    NSLog(@"key=%@ expected_value=%@ actual_value=%@", key, expected_value,
          actual_value);

    if ([expected_value isKindOfClass:[NSNumber class]]) {
      // bool and int values are stored in the expected results dict as
      // NSNumber; just value check via -intValue.
      XCTAssertEqual([expected_value intValue], [actual_value intValue]);
    } else if ([expected_value isKindOfClass:[NSString class]]) {
      XCTAssertTrue([actual_value isEqualToString:expected_value]);
    } else {
      XCTFail(@"Unexpected object type in values from expectedParseResults!");
    }
  }
}

// Test parsing of testdata/manifest_vp9_vorbis.mpd.
- (void)_testParseDASHMPD1 {
  NSURL* const manifest_url = [NSURL URLWithString:kVP9VorbisDASHMPD1URLString];
  IxoDASHManifestParser* parser =
      [[IxoDASHManifestParser alloc] initWithManifestURL:manifest_url];
  const bool parse_ok = [parser parse];
  XCTAssertTrue(parse_ok);

  // Verify MPD attributes.
  IxoDASHManifest* const manifest = parser.manifest;
  XCTAssertEqual(manifest.staticPresentation, kVP9VorbisDASHMPD1IsStatic);
  XCTAssertTrue([manifest.mediaPresentationDuration
      isEqualToString:kVP9VorbisDASHMPD1MediaPresentationDuration]);
  XCTAssertTrue(
      [manifest.minBufferTime isEqualToString:kVP9VorbisDASHMPD1MinBufferTime]);

  // Verify Period attributes.
  IxoDASHPeriod* const period = manifest.period;
  XCTAssertTrue([period.periodID isEqualToString:kVP9VorbisDASHMPD1PeriodID]);
  XCTAssertTrue([period.start isEqualToString:kVP9VorbisDASHMPD1PeriodStart]);
  XCTAssertTrue(
      [period.duration isEqualToString:kVP9VorbisDASHMPD1PeriodDuration]);

  // Verify audio AdaptationSet attributes.
  XCTAssertEqual([period.audioAdaptationSets count],
                 kVP9VorbisDASHMPD1AudioASCount);
  IxoDASHAdaptationSet* const audio_as =
      [period.audioAdaptationSets lastObject];
  XCTAssertTrue([audio_as.setID isEqualToString:kVP9VorbisDASHMPD1AudioASID]);
  XCTAssertTrue(
      [audio_as.codecs isEqualToString:kVP9VorbisDASHMPD1AudioASCodecs]);
  XCTAssertTrue(
      [audio_as.mimeType isEqualToString:kVP9VorbisDASHMPD1AudioASMimeType]);
  XCTAssertEqual(audio_as.audioSamplingRate,
                 kVP9VorbisDASHMPD1AudioASAudioSamplingRate);

  // Verify first audio representation attributes.
  XCTAssertEqual([audio_as.representations count],
                 kVP9VorbisDASHMPD1AudioRepCount);
  IxoDASHRepresentation* const audio_rep =
      [audio_as.representations firstObject];
  XCTAssertTrue(
      [audio_rep.repID isEqualToString:kVP9VorbisDASHMPD1AudioRep0ID]);
  XCTAssertTrue([audio_rep.baseURL
      isEqualToString:kVP9VorbisDASHMPD1AudioRep0BaseURLString]);
  XCTAssertEqual([audio_rep.segmentBaseIndexRange count],
                 kNumElementsInRangeArray);
  XCTAssertEqual([[audio_rep.segmentBaseIndexRange firstObject] intValue],
                 kVP9VorbisDASHMPD1AudioRep0IndexRangeStart);
  XCTAssertEqual([[audio_rep.segmentBaseIndexRange lastObject] intValue],
                 kVP9VorbisDASHMPD1AudioRep0IndexRangeEnd);
  XCTAssertEqual([[audio_rep.initializationRange firstObject] intValue],
                 kVP9VorbisDASHMPD1AudioRep0InitRangeStart);
  XCTAssertEqual([[audio_rep.initializationRange lastObject] intValue],
                 kVP9VorbisDASHMPD1AudioRep0InitRangeEnd);
  XCTAssertEqual(audio_rep.bandwidth, kVP9VorbisDASHMPD1AudioRep0Bandwidth);

  // Verify video AdaptationSet attributes.
  XCTAssertEqual([period.videoAdaptationSets count],
                 kVP9VorbisDASHMPD1VideoASCount);
  IxoDASHAdaptationSet* const video_as =
      [period.videoAdaptationSets lastObject];
  XCTAssertTrue([video_as.setID isEqualToString:kVP9VorbisDASHMPD1VideoASID]);
  XCTAssertTrue(
      [video_as.codecs isEqualToString:kVP9VorbisDASHMPD1VideoASCodecs]);
  XCTAssertTrue(
      [video_as.mimeType isEqualToString:kVP9VorbisDASHMPD1VideoASMimeType]);
  XCTAssertEqual(video_as.width, kVP9VorbisDASHMPD1VideoASWidth);
  XCTAssertEqual(video_as.height, kVP9VorbisDASHMPD1VideoASHeight);

  // Verify first audio representation attributes.
  XCTAssertEqual([video_as.representations count],
                 kVP9VorbisDASHMPD1VideoRepCount);
  IxoDASHRepresentation* const video_rep =
      [video_as.representations firstObject];
  XCTAssertTrue(
      [video_rep.repID isEqualToString:kVP9VorbisDASHMPD1VideoRep0ID]);
  XCTAssertTrue([video_rep.baseURL
      isEqualToString:kVP9VorbisDASHMPD1VideoRep0BaseURLString]);
  XCTAssertEqual([video_rep.segmentBaseIndexRange count],
                 kNumElementsInRangeArray);
  XCTAssertEqual([[video_rep.segmentBaseIndexRange firstObject] intValue],
                 kVP9VorbisDASHMPD1VideoRep0IndexRangeStart);
  XCTAssertEqual([[video_rep.segmentBaseIndexRange lastObject] intValue],
                 kVP9VorbisDASHMPD1VideoRep0IndexRangeEnd);
  XCTAssertEqual([[video_rep.initializationRange firstObject] intValue],
                 kVP9VorbisDASHMPD1VideoRep0InitRangeStart);
  XCTAssertEqual([[video_rep.initializationRange lastObject] intValue],
                 kVP9VorbisDASHMPD1VideoRep0InitRangeEnd);
  XCTAssertEqual(video_rep.bandwidth, kVP9VorbisDASHMPD1VideoRep0Bandwidth);
}

@end
