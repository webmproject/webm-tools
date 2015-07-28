// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoDASHManifestParser.h"

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "IxoPlayerTestCommon.h"

@interface IxoDASHManifestParserTests : XCTestCase
@end

@implementation IxoDASHManifestParserTests {
}

// Note: -setUp and -tearDown are called for each test.

- (void)setUp {
  [super setUp];
}

- (void)tearDown {
  [super tearDown];
}

- (void)testSandbox {
  NSURL* const manifest_url = [NSURL URLWithString:kStaticManifestURLString];
  IxoDASHManifestParser* parser =
      [[IxoDASHManifestParser alloc] initWithManifestURL:manifest_url];
  bool parsed = [parser parse];
  XCTAssert(parsed == true);
}

@end  // implementation IxoDASHManifestParserTests
