// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoDownloadQueue.h"

NSString* const kDownloadQueueName = @"IxoDownloadQueue";
const int kMaxConcurrentDownloads = 3;

@implementation IxoDownloadQueue

@synthesize downloadQueue = _downloadQueue;
@synthesize activeDownloads = _activeDownloads;

- (NSOperationQueue*)downloadQueue {
  if (_downloadQueue == nil) {
    _downloadQueue = [[NSOperationQueue alloc] init];
    _downloadQueue.name = kDownloadQueueName;
    _downloadQueue.maxConcurrentOperationCount = kMaxConcurrentDownloads;
  }
  return _downloadQueue;
}

- (NSMutableDictionary*)activeDownloads {
  if (_activeDownloads == nil) {
    _activeDownloads = [[NSMutableDictionary alloc] init];
  }
  return _activeDownloads;
}

@end
