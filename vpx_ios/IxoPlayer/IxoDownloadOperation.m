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

  if (record.requestedRange != nil) {
    const NSNumber* const range_begin = record.requestedRange[0];
    const NSNumber* const range_end = record.requestedRange[1];

    NSString* const rangeString =
        [NSString stringWithFormat:@"bytes=%d-%d", [range_begin intValue],
                                   [range_end intValue]];
    [request setValue:rangeString forHTTPHeaderField:@"range"];
  }

  AFHTTPRequestOperation* request_op =
      [[AFHTTPRequestOperation alloc] initWithRequest:request];
  return request_op;
}

- (void)forwardResponseRecordToDataSource:(IxoDownloadRecord*)record {
  if (record.dataSource != nil)
    [record.dataSource downloadCompleteForDownloadRecord:record];
}

- (void)main {
  if (_record == nil || self.isCancelled) {
    return;
  }

  AFHTTPRequestOperation* const download_operation =
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
  _record.failed = (download_operation.error != nil ||
                    download_operation.responseData == nil);

  if (_record.requestedRange != nil && !_record.failed) {
    const NSNumber* const range_begin = _record.requestedRange[0];
    const NSNumber* const range_end = _record.requestedRange[1];
    const int expected_length =
        1 + [range_end intValue] - [range_begin intValue];

    if (_record.data.length > expected_length) {
      // More data returned than requested. This is likely due to local caching,
      // but the main point is that the data that's actually been requested must
      // be extracted from the blob returned.
      const uint8_t* const data = (uint8_t*)[_record.data bytes];
      const uint8_t* const requested_data = data + [range_begin intValue];

      NSData* const response =
          [NSData dataWithBytes:requested_data length:expected_length];
      _record.data = response;
    }
    _record.failed = _record.data.length != expected_length;
  }

  [self forwardResponseRecordToDataSource:_record];
}

@end
