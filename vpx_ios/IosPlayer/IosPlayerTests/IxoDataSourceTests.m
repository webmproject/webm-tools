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
#import "IosPlayerTestCommon.h"

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
  CC_MD5(string_data, kVP9VorbisDASHMPD1Length, digest);
  NSMutableString* md5_string = [[NSMutableString alloc] init];

  for (int i = 0; i < CC_MD5_DIGEST_LENGTH; i++)
    [md5_string appendFormat:@"%02x", digest[i]];

  return [expectedMD5 isEqualToString:md5_string] == YES;
}

- (NSMutableArray*)getRangeArrayFromIntsWithBeginOffset:(int)begin
                                              endOffset:(int)end {
  return [NSMutableArray arrayWithObjects:[NSNumber numberWithInt:begin],
                                          [NSNumber numberWithInt:end], nil];
}

- (void)testSynchronousDownload {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];

  IxoDownloadRecord* record = [data_source
      downloadFromURL:[NSURL URLWithString:kVP9VorbisDASHMPD1URLString]];

  // Make sure the download record doesn't have a resourceLength-- this should
  // always be 0 for a non-ranged download.
  XCTAssert(record.resourceLength == 0);

  // Make sure data was received and is correct.
  NSData* const manifest_data = record.data;
  XCTAssertNotNil(manifest_data, @"Nil manifest.");
  XCTAssertEqual(manifest_data.length, kVP9VorbisDASHMPD1Length,
                 @"Manifest length incorrect.");
  XCTAssert([self MD5SumForData:manifest_data
                matchesMD5SumString:kVP9VorbisDASHMPD1MD5],
            @"MD5 mismatch");
}

- (void)testSynchronousDownloadWithRange {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];

  const int range_end = kVP9VorbisDASHMPD1MiddleLineOffset +
                        kVP9VorbisDASHMPD1MiddleLineLength - 1;
  NSArray* const range_array = [self
      getRangeArrayFromIntsWithBeginOffset:kVP9VorbisDASHMPD1MiddleLineOffset
                                 endOffset:range_end];
  IxoDownloadRecord* record = [data_source
      downloadFromURL:[NSURL URLWithString:kVP9VorbisDASHMPD1URLString]
            withRange:range_array];

  // Make sure the resourceLength field in |record| contains the correct file
  // size.
  XCTAssert(record.resourceLength == kVP9VorbisDASHMPD1Length);

  // Make sure data was received.
  NSData* manifest_data = record.data;
  NSString* response_string =
      [[NSString alloc] initWithData:manifest_data
                            encoding:NSUTF8StringEncoding];
  XCTAssert([response_string isEqualToString:kVP9VorbisDASHMPD1MiddleLine]);
  XCTAssertEqual(manifest_data.length, kVP9VorbisDASHMPD1MiddleLineLength);
}

- (void)testMultiSynchronousDownloadWithRange {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];
  int range_end = kVP9VorbisDASHMPD1MiddleLineOffset +
                  kVP9VorbisDASHMPD1MiddleLineLength - 1;
  NSMutableArray* const range_array = [self
      getRangeArrayFromIntsWithBeginOffset:kVP9VorbisDASHMPD1MiddleLineOffset
                                 endOffset:range_end];
  NSMutableData* const middle_data =
      [[data_source
           downloadFromURL:[NSURL URLWithString:kVP9VorbisDASHMPD1URLString]
                 withRange:range_array]
              .data copy];

  range_array[0] = [NSNumber numberWithInt:0];
  range_array[1] =
      [NSNumber numberWithInt:kVP9VorbisDASHMPD1FirstLineLength - 1];
  NSMutableData* const first_data =
      [[data_source
           downloadFromURL:[NSURL URLWithString:kVP9VorbisDASHMPD1URLString]
                 withRange:range_array]
              .data copy];

  // Make sure data was received.
  NSString* const middle_line =
      [[NSString alloc] initWithData:middle_data encoding:NSUTF8StringEncoding];
  NSString* const first_line =
      [[NSString alloc] initWithData:first_data encoding:NSUTF8StringEncoding];
  NSString* const combined_lines =
      [NSString stringWithFormat:@"%@%@", middle_line, first_line, nil];
  NSString* const expected_string =
      [NSString stringWithFormat:@"%@%@", kVP9VorbisDASHMPD1MiddleLine,
                                 kVP9VorbisDASHMPD1FirstLine, nil];
  XCTAssert([expected_string isEqualToString:combined_lines]);
  XCTAssertEqual(combined_lines.length, kVP9VorbisDASHMPD1MiddleLineLength +
                                            kVP9VorbisDASHMPD1FirstLineLength);
}

- (void)testAsyncDownload {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];

  [_dataReceivedCondition lock];

  const int download_id = [data_source
      downloadDataFromURL:[NSURL URLWithString:kVP9VorbisDASHMPD1URLString]
                withRange:nil
               toListener:self];

  // Wait for response.
  while (_downloadRecord == nil)
    [_dataReceivedCondition wait];

  XCTAssertEqual(download_id, _downloadRecord.downloadID);
  XCTAssert(_downloadRecord.failed == false, @"Download failed");
  XCTAssertNotNil(_downloadRecord.data, @"No response data");
  XCTAssert([self MD5SumForData:_downloadRecord.data
                matchesMD5SumString:kVP9VorbisDASHMPD1MD5],
            @"MD5 mismatch");
  XCTAssertNil(_downloadRecord.error, @"Record has non nil error");
  [_dataReceivedCondition unlock];
}

- (void)testRangedAsyncDownloadFirstLine {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];

  [_dataReceivedCondition lock];
  NSArray* const range_array = [self
      getRangeArrayFromIntsWithBeginOffset:0
                                 endOffset:kVP9VorbisDASHMPD1FirstLineLength -
                                           1];
  const int download_id = [data_source
      downloadDataFromURL:[NSURL URLWithString:kVP9VorbisDASHMPD1URLString]
                withRange:range_array
               toListener:self];

  // Wait for response.
  while (_downloadRecord == nil)
    [_dataReceivedCondition wait];

  NSString* response_string =
      [[NSString alloc] initWithData:_downloadRecord.data
                            encoding:NSUTF8StringEncoding];
  XCTAssert([response_string isEqualToString:kVP9VorbisDASHMPD1FirstLine]);
  XCTAssertEqual(_downloadRecord.data.length,
                 kVP9VorbisDASHMPD1FirstLineLength);
  XCTAssertEqual(download_id, _downloadRecord.downloadID);
  XCTAssert(_downloadRecord.failed == false, @"Download failed");
  XCTAssertNotNil(_downloadRecord.data, @"No response data");
  XCTAssertNil(_downloadRecord.error, @"Record has non nil error");
  [_dataReceivedCondition unlock];
}

- (void)testRangedAsyncDownloadMiddleLine {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];

  [_dataReceivedCondition lock];
  const int range_end = kVP9VorbisDASHMPD1MiddleLineOffset +
                        kVP9VorbisDASHMPD1MiddleLineLength - 1;
  NSArray* const range_array = [self
      getRangeArrayFromIntsWithBeginOffset:kVP9VorbisDASHMPD1MiddleLineOffset
                                 endOffset:range_end];
  const int download_id = [data_source
      downloadDataFromURL:[NSURL URLWithString:kVP9VorbisDASHMPD1URLString]
                withRange:range_array
               toListener:self];

  // Wait for response.
  while (_downloadRecord == nil)
    [_dataReceivedCondition wait];

  NSString* response_string =
      [[NSString alloc] initWithData:_downloadRecord.data
                            encoding:NSUTF8StringEncoding];
  XCTAssert([response_string isEqualToString:kVP9VorbisDASHMPD1MiddleLine]);
  XCTAssertEqual(_downloadRecord.data.length,
                 kVP9VorbisDASHMPD1MiddleLineLength);
  XCTAssertEqual(download_id, _downloadRecord.downloadID);
  XCTAssert(_downloadRecord.failed == false, @"Download failed");
  XCTAssertNotNil(_downloadRecord.data, @"No response data");
  XCTAssertNil(_downloadRecord.error, @"Record has non nil error");
  [_dataReceivedCondition unlock];
}

- (void)testRangedAsyncDownloadLastLine {
  IxoDataSource* data_source = [[IxoDataSource alloc] init];

  [_dataReceivedCondition lock];
  const int range_end =
      kVP9VorbisDASHMPD1LastLineOffset + kVP9VorbisDASHMPD1LastLineLength - 1;
  NSArray* const range_array = [self
      getRangeArrayFromIntsWithBeginOffset:kVP9VorbisDASHMPD1LastLineOffset
                                 endOffset:range_end];
  const int download_id = [data_source
      downloadDataFromURL:[NSURL URLWithString:kVP9VorbisDASHMPD1URLString]
                withRange:range_array
               toListener:self];

  // Wait for response.
  while (_downloadRecord == nil)
    [_dataReceivedCondition wait];

  NSString* response_string =
      [[NSString alloc] initWithData:_downloadRecord.data
                            encoding:NSUTF8StringEncoding];
  XCTAssert([response_string isEqualToString:kVP9VorbisDASHMPD1LastLine]);
  XCTAssertEqual(_downloadRecord.data.length, kVP9VorbisDASHMPD1LastLineLength);
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
