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

#import "IxoDASHManifestTestData.h"
#import "IxoPlayerTestCommon.h"

@interface IxoDASHManifestParserTests : XCTestCase
@end

@implementation IxoDASHManifestParserTests

//
// XCTest methods.
//
- (void)setUp {
  [super setUp];
}

- (void)tearDown {
  [super tearDown];
}

//
// IxoDASHManifestParser parser test helpers.
//

- (bool)idIsArrayType:(id)unknownType {
  return [unknownType isKindOfClass:[NSArray class]];
}

- (bool)idIsStringType:(id)unknownType {
  return [unknownType isKindOfClass:[NSString class]];
}

- (bool)arrayMatches:(NSArray*)arr array:(NSArray*)otherArray {
  return [arr isEqualToArray:otherArray] == YES;
}

- (bool)stringMatches:(NSString*)str string:(NSString*)otherStr {
  return [str isEqualToString:otherStr] == YES;
}

// Returns array of property names for given type id.
- (NSArray*)allPropertyNamesForType:(id)type {
  uint num_props;
  objc_property_t* const properties =
      class_copyPropertyList([type class], &num_props);
  NSMutableArray* prop_names = [NSMutableArray array];

  for (uint i = 0; i < num_props; ++i) {
    objc_property_t property = properties[i];
    NSString* const name =
        [NSString stringWithUTF8String:property_getName(property)];
    [prop_names addObject:name];
  }
  free(properties);
  NSLog(@"allPropertyNamesForType: returning %@", prop_names);
  return prop_names;
}

// Returns true when representations match, or dies.
- (bool)representationMatches:(IxoDASHRepresentation*)rep
       ExpectedRepresentation:(IxoMutableDASHRepresentation*)expectedRep {
  NSArray* const props =
      [self allPropertyNamesForType:[IxoDASHRepresentation class]];
  for (NSString* const prop in props) {
    // Compare the prop values.
    if ([self idIsArrayType:[rep valueForKey:prop]]) {
      XCTAssertTrue([self arrayMatches:[rep valueForKey:prop]
                                 array:[expectedRep valueForKey:prop]],
                    @"value not equal for prop: %@", prop);
    } else if ([self idIsStringType:[rep valueForKey:prop]]) {
      XCTAssertTrue([self stringMatches:[rep valueForKey:prop]
                                 string:[expectedRep valueForKey:prop]],
                    @"value not equal for prop: %@", prop);

    } else {
      XCTAssertEqual([rep valueForKey:prop], [expectedRep valueForKey:prop],
                     @"value not equal for prop: %@", prop);
    }
  }
  return true;
}

// Returns true when AdaptationSets match, or dies.
- (bool)adaptationSetMatches:(IxoDASHAdaptationSet*)set
       ExpectedAdaptationSet:(IxoMutableDASHAdaptationSet*)expectedSet {
  NSArray* const props =
      [self allPropertyNamesForType:[IxoDASHAdaptationSet class]];

  // Compare AdaptationSet attributes.
  for (NSString* const prop in props) {
    // Representation(s) check is done below; skip it here.
    if ([prop isEqualToString:@"representations"]) {
      continue;
    }

    // Compare the prop values.
    if ([self idIsStringType:[set valueForKey:prop]]) {
      XCTAssertTrue([self stringMatches:[set valueForKey:prop]
                                 string:[expectedSet valueForKey:prop]],
                    @"value not equal for prop: %@", prop);
    } else {
      XCTAssertEqual([set valueForKey:prop], [expectedSet valueForKey:prop],
                     @"value not equal for prop: %@", prop);
    }
  }

  // Compare the Representation elements from the AdaptationSet.
  XCTAssertEqual(set.representations.count, expectedSet.representations.count);
  for (uint i = 0; i < set.representations.count; ++i) {
    XCTAssertTrue([self
         representationMatches:[set.representations objectAtIndex:i]
        ExpectedRepresentation:[expectedSet.representations objectAtIndex:i]]);
  }
  return true;
}

// Returns true when Periods match, or dies.
- (bool)periodMatches:(IxoDASHPeriod*)period
       ExpectedPeriod:(IxoMutableDASHPeriod*)expectedPeriod {
  NSArray* const props = [self allPropertyNamesForType:[IxoDASHPeriod class]];

  // Compare Period attributes.
  for (NSString* const prop in props) {
    // {audio|video}AdaptationSets are checked below; skip the checks here.
    if ([prop isEqualToString:@"audioAdaptationSets"] ||
        [prop isEqualToString:@"videoAdaptationSets"]) {
      continue;
    }

    XCTAssertTrue([[period valueForKey:prop]
                      isEqualToString:[expectedPeriod valueForKey:prop]],
                  @"value not equal for prop: %@", prop);
  }

  // Compare the audio AdaptationSet elements from the Period.
  XCTAssertEqual(period.audioAdaptationSets.count,
                 expectedPeriod.audioAdaptationSets.count);
  for (uint i = 0; i < period.audioAdaptationSets.count; ++i) {
    XCTAssertTrue(
        [self adaptationSetMatches:[period.audioAdaptationSets objectAtIndex:i]
             ExpectedAdaptationSet:[expectedPeriod.audioAdaptationSets
                                       objectAtIndex:i]]);
  }

  // Compare the video AdaptationSet elements from the Period.
  XCTAssertEqual(period.videoAdaptationSets.count,
                 expectedPeriod.videoAdaptationSets.count);
  for (uint i = 0; i < period.videoAdaptationSets.count; ++i) {
    XCTAssertTrue(
        [self adaptationSetMatches:[period.videoAdaptationSets objectAtIndex:i]
             ExpectedAdaptationSet:[expectedPeriod.videoAdaptationSets
                                       objectAtIndex:i]]);
  }

  return true;
}

// Returns true when manifests match, or dies.
- (bool)manifestMatches:(IxoDASHManifest*)manifest
       ExpectedManifest:(IxoMutableDASHManifest*)expectedManifest {
  NSArray* const props = [self allPropertyNamesForType:[IxoDASHManifest class]];

  // Compare manifest attributes.
  for (NSString* const prop in props) {
    // Period check is done below; skip it here.
    if ([prop isEqualToString:@"period"]) {
      continue;
    }

    if ([self idIsStringType:[manifest valueForKey:prop]]) {
      XCTAssertTrue([self stringMatches:[manifest valueForKey:prop]
                                 string:[expectedManifest valueForKey:prop]],
                    @"value not equal for prop: %@", prop);
    } else {
      XCTAssertEqual([manifest valueForKey:prop],
                     [expectedManifest valueForKey:prop],
                     @"value not equal for prop: %@", prop);
    }
  }

  // Compare the Representation elements from the AdaptationSet.
  XCTAssertTrue([self periodMatches:manifest.period
                     ExpectedPeriod:expectedManifest.period]);
  return true;
}

//
// IxoDASHManifestParser tests.
//
- (void)testParseDASH1 {
  NSURL* const manifest_url = [NSURL URLWithString:kVP9VorbisDASHMPD1URLString];
  IxoDASHManifestParser* parser =
      [[IxoDASHManifestParser alloc] initWithManifestURL:manifest_url];
  const bool parse_ok = [parser parse];
  XCTAssertTrue(parse_ok);

  IxoDASHManifest* const manifest = parser.manifest;
  IxoMutableDASHManifest* const expected_manifest = [IxoDASHManifestTestData
      getExpectedManifestForURLString:kVP9VorbisDASHMPD1URLString];
  XCTAssertTrue(
      [self manifestMatches:manifest ExpectedManifest:expected_manifest]);
}
@end
