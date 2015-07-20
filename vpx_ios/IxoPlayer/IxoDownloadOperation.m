// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoDownloadOperation.h"

#import "AFNetworking/AFNetworking.h"

#import "IxoDownloadRecord.h"

@implementation IxoDownloadOperation {
  IxoDownloadRecord* _record;
}

- (instancetype)initWithDownloadRecord:(IxoDownloadRecord*)record {
  if ((self = [super init])) {
    _record = record;
  }
  return self;
}

// Builds an AFHTTPRequestOperation from an IxoDownloadRecord. The |record| must
// outlive the download operation. This method is asynchronous. Success and
// failure are reported via the |listener| stored in |record| via call to
// -forwardResponseRecordToDataSource:.
- (AFHTTPRequestOperation*)createHTTPRequestFromDownloadRecord:
    (IxoDownloadRecord*)record {
  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:record.URL];

  // NO LOCAL CACHING! Why:
  // This is a feature of iOS. When the same resource is requested repeatedly,
  // it's cached locally (this is good). When a ranged request is sent for that
  // same resource, but we only want bytes 100-150 in the resource we previously
  // asked for, the built in cache mechanism returns the full resource and
  // ignores the range request (this is VERY VERY BAD). The way to avoid this
  // behavior is to disable it entirely:
  [request setCachePolicy:NSURLRequestReloadIgnoringLocalCacheData];

  if (record.requestedRange != nil) {
    NSNumber* range_begin = record.requestedRange[0];
    NSNumber* range_end = record.requestedRange[1];

    NSString* rangeString =
        [NSString stringWithFormat:@"bytes=%d-%d", [range_begin intValue],
                                   [range_end intValue]];
    [request setValue:rangeString forHTTPHeaderField:@"range"];
  }

  AFHTTPRequestOperation* request_op =
      [[AFHTTPRequestOperation alloc] initWithRequest:request];
  return request_op;
}

- (void)forwardResponseRecordToDataSource:(IxoDownloadRecord*)record {
  [record.dataSource downloadCompleteForDownloadRecord:record];
}

- (void)main {
  if (_record == nil || self.isCancelled) {
    return;
  }

  AFHTTPRequestOperation* download_operation =
      [self createHTTPRequestFromDownloadRecord:_record];
  [download_operation start];

  if (self.isCancelled) {
    return;
  }

  [download_operation waitUntilFinished];

  if (self.isCancelled) {
    return;
  }

  _record.data = download_operation.responseData;
  _record.error = download_operation.error;
  _record.failed = download_operation.error != nil;

  if (_record.requestedRange != nil && !_record.failed) {
    NSNumber* range_begin = _record.requestedRange[0];
    NSNumber* range_end = _record.requestedRange[1];
    const int expected_length =
        1 + [range_end intValue] - [range_begin intValue];
    _record.failed = _record.data.length != expected_length;
  }

  [self forwardResponseRecordToDataSource:_record];
}

@end
