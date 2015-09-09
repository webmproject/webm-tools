// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoDataSource.h"
#import "IxoDownloadOperation.h"
#import "IxoDownloadQueue.h"
#import "IxoDownloadRecord.h"

@implementation IxoDataSource {
  int _downloadID;
  IxoDownloadQueue* _downloadQueue;
  NSLock* _lock;
}

- (id)init {
  self = [super init];

  if (self) {
    _downloadID = 0;
    _downloadQueue = [[IxoDownloadQueue alloc] init];
    _lock = [[NSLock alloc] init];

    if (_downloadQueue == nil || _lock == nil)
      return nil;
  }

  return self;
}

- (NSData*)downloadFromURL:(NSURL*)URL {
  // TODO(tomfinegan): Not sure if this will work w/HTTPS.
  return [[NSData alloc] initWithContentsOfURL:URL];
}

- (int)downloadDataFromURL:(NSURL*)URL
                 withRange:(NSArray*)range
                toListener:(id)listener {

  int download_id = -1;

  [_lock lock];
  download_id = _downloadID++;
  [_lock unlock];

  IxoDownloadRecord* record = [[IxoDownloadRecord alloc] init];
  if (record == nil) {
    NSLog(@"downloadDataFromURL: out of memory.");
    return -1;
  }

  record.URL = URL;
  record.requestedRange = range;
  record.listener = listener;
  record.downloadID = download_id;
  record.dataSource = self;

  IxoDownloadOperation* download_op =
      [[IxoDownloadOperation alloc] initWithDownloadRecord:record];
  if (download_op == nil) {
    NSLog(@"downloadDataFromURL: out of memory.");
    return -1;
  }

  [_lock lock];
  [_downloadQueue.activeDownloads
      setObject:record
         forKey:[NSNumber numberWithInt:download_id]];
  [_downloadQueue.downloadQueue addOperation:download_op];
  [_lock unlock];

  return download_id;
}

- (void)downloadCompleteForDownloadRecord:(IxoDownloadRecord*)record {
  NSLog(@"Download complete for %d:%@", record.downloadID,
        [record.URL absoluteString]);
  [_lock lock];
  [_downloadQueue.activeDownloads
      removeObjectForKey:[NSNumber numberWithInt:record.downloadID]];
  [_lock unlock];
  NSLog(@"%d:%@ removed from activeDownloads.", record.downloadID,
        [record.URL absoluteString]);
  [record.listener receiveData:record];
}

@end
