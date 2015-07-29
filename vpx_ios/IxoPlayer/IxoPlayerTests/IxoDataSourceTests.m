// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import <CommonCrypto/CommonDigest.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "IxoDownloadRecord.h"
#import "IxoDataSource.h"
#import "IxoPlayerTestCommon.h"

// Expected values for data length and MD5 checksum (of the entire MPD file).
const int kExpectedManifestLength = 2005;
NSString* const kExpectedMD5 = @"8ab746aecdc021ef92d9162f4ba5dd89";

// Line lengths and their offsets (when non-zero) for ranged request tests.
const int kManifestFirstLineLength = 38;
NSString* const kManifestFirstLine =
    @"<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

const int kManifestMiddleLineLength = 15;
const int kManifestMiddleLineOffset = 1108;
NSString* const kManifestMiddleLine = @"<Initialization";

const int kManifestLastLineLength = 6;
const int kManifestLastLineOffset = 1998;
NSString* const kManifestLastLine = @"</MPD>";

@interface IxoDataSourceTests : XCTestCase<IxoDataSourceListener>
@end

@implementation IxoDataSourceTests {
  NSCondition* _dataReceivedCondition;
  IxoDownloadRecord* _downloadRecord;
}

// Note: -setUp and -tearDown are called for each test.

- (void)setUp {
  _dataReceivedCondition = [[NSCondition alloc] init];
  [super setUp];
}

- (void)tearDown {
  _dataReceivedCondition = nil;
  _downloadRecord = nil;
  [super tearDown];
}

- (bool)MD5SumForData:(NSData*)data matchesMD5SumString:(NSString*)expectedMD5 {
  NSString* data_string =
      [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];

  // Confirm data matches expectation.
  const char* string_data = [data_string UTF8String];
  unsigned char digest[CC_MD5_DIGEST_LENGTH];
  CC_MD5(string_data, kExpectedManifestLength, digest);
  NSMutableString* md5_string = [[NSMutableString alloc] init];

  for (int i = 0; i < CC_MD5_DIGEST_LENGTH; i++)
    [md5_string appendFormat:@"%02x", digest[i]];

  return [expectedMD5 isEqualToString:md5_string] == YES;
}

- (NSArray*)getRangeArrayFromIntsWithBeginOffset:(int)begin endOffset:(int)end {
  return [NSArray arrayWithObjects:[NSNumber numberWithInt:begin],
                                   [NSNumber numberWithInt:end], nil];
}

- (void)testSynchronousDownload {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];
  NSData* manifest_data = [data_source
      downloadFromURL:[NSURL URLWithString:kStaticManifestURLString]];

  // Make sure data was received.
  XCTAssertNotNil(manifest_data, @"Nil manifest.");
  XCTAssertEqual(manifest_data.length, kExpectedManifestLength,
                 @"Manifest length incorrect.");

  XCTAssert([self MD5SumForData:manifest_data matchesMD5SumString:kExpectedMD5],
            @"MD5 mismatch");
}

- (void)testAsyncDownload {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];

  [_dataReceivedCondition lock];

  const int download_id = [data_source
      downloadDataFromURL:[NSURL URLWithString:kStaticManifestURLString]
                withRange:nil
               toListener:self];

  // Wait for response.
  while (_downloadRecord == nil)
    [_dataReceivedCondition wait];

  XCTAssertEqual(download_id, _downloadRecord.downloadID);
  XCTAssert(_downloadRecord.failed == false, @"Download failed");
  XCTAssertNotNil(_downloadRecord.data, @"No response data");
  XCTAssert([self MD5SumForData:_downloadRecord.data
                matchesMD5SumString:kExpectedMD5],
            @"MD5 mismatch");
  XCTAssertNil(_downloadRecord.error, @"Record has non nil error");
  [_dataReceivedCondition unlock];
}

- (void)testRangedAsyncDownloadFirstLine {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];

  [_dataReceivedCondition lock];
  NSArray* const range_array =
      [self getRangeArrayFromIntsWithBeginOffset:0
                                       endOffset:kManifestFirstLineLength - 1];
  const int download_id = [data_source
      downloadDataFromURL:[NSURL URLWithString:kStaticManifestURLString]
                withRange:range_array
               toListener:self];

  // Wait for response.
  while (_downloadRecord == nil)
    [_dataReceivedCondition wait];

  NSString* response_string =
      [[NSString alloc] initWithData:_downloadRecord.data
                            encoding:NSUTF8StringEncoding];
  XCTAssert([response_string isEqualToString:kManifestFirstLine]);
  XCTAssertEqual(_downloadRecord.data.length, kManifestFirstLineLength);
  XCTAssertEqual(download_id, _downloadRecord.downloadID);
  XCTAssert(_downloadRecord.failed == false, @"Download failed");
  XCTAssertNotNil(_downloadRecord.data, @"No response data");
  XCTAssertNil(_downloadRecord.error, @"Record has non nil error");
  [_dataReceivedCondition unlock];
}

- (void)testRangedAsyncDownloadMiddleLine {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];

  [_dataReceivedCondition lock];
  const int range_end =
      kManifestMiddleLineOffset + kManifestMiddleLineLength - 1;
  NSArray* const range_array =
      [self getRangeArrayFromIntsWithBeginOffset:kManifestMiddleLineOffset
                                       endOffset:range_end];
  const int download_id = [data_source
      downloadDataFromURL:[NSURL URLWithString:kStaticManifestURLString]
                withRange:range_array
               toListener:self];

  // Wait for response.
  while (_downloadRecord == nil)
    [_dataReceivedCondition wait];

  NSString* response_string =
      [[NSString alloc] initWithData:_downloadRecord.data
                            encoding:NSUTF8StringEncoding];
  XCTAssert([response_string isEqualToString:kManifestMiddleLine]);
  XCTAssertEqual(_downloadRecord.data.length, kManifestMiddleLineLength);
  XCTAssertEqual(download_id, _downloadRecord.downloadID);
  XCTAssert(_downloadRecord.failed == false, @"Download failed");
  XCTAssertNotNil(_downloadRecord.data, @"No response data");
  XCTAssertNil(_downloadRecord.error, @"Record has non nil error");
  [_dataReceivedCondition unlock];
}

- (void)testRangedAsyncDownloadLastLine {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];

  [_dataReceivedCondition lock];
  const int range_end = kManifestLastLineOffset + kManifestLastLineLength - 1;
  NSArray* const range_array =
      [self getRangeArrayFromIntsWithBeginOffset:kManifestLastLineOffset
                                       endOffset:range_end];
  const int download_id = [data_source
      downloadDataFromURL:[NSURL URLWithString:kStaticManifestURLString]
                withRange:range_array
               toListener:self];

  // Wait for response.
  while (_downloadRecord == nil)
    [_dataReceivedCondition wait];

  NSString* response_string =
      [[NSString alloc] initWithData:_downloadRecord.data
                            encoding:NSUTF8StringEncoding];
  XCTAssert([response_string isEqualToString:kManifestLastLine]);
  XCTAssertEqual(_downloadRecord.data.length, kManifestLastLineLength);
  XCTAssertEqual(download_id, _downloadRecord.downloadID);
  XCTAssert(_downloadRecord.failed == false, @"Download failed");
  XCTAssertNotNil(_downloadRecord.data, @"No response data");
  XCTAssertNil(_downloadRecord.error, @"Record has non nil error");
  [_dataReceivedCondition unlock];
}

- (void)receiveData:(IxoDownloadRecord*)downloadRecord {
  _downloadRecord = downloadRecord;
  [_dataReceivedCondition signal];
}

@end
