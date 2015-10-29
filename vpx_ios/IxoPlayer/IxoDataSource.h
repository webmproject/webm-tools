// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#import <Foundation/Foundation.h>

@class IxoDownloadRecord;

// Listener protocol for asynchronous downloads.
@protocol IxoDataSourceListener
// Called upon download completion or failure. Status contained within
// |downloadRecord|.
- (void)receiveData:(IxoDownloadRecord*)downloadRecord;
@end

@interface IxoDataSource : NSObject

// Returns all bytes from |URL|. Synchronous. Returns nil or the data from
// |URL|.
- (NSData*)downloadFromURL:(NSURL*)URL;

// Returns |range| bytes from |URL|. Synchronous. Returns nil or the data from
// |URL|.
- (NSData*)downloadFromURL:(NSURL*)URL withRange:(NSArray*)range;

// Downloads bytes specified by |range| and sends them to |listener|. This
// method is asynchronous; the download has started when it returns any
// non-negative number. Non-negative return values are the identifier for the
// download started by the successful
// -downloadDataFromURL:withRange:toListener call.
// The |listener| is notified after completion of download; not while it is in
// progress.
- (int)downloadDataFromURL:(NSURL*)URL
                 withRange:(NSArray*)range
                toListener:(id)listener;

- (void)downloadCompleteForDownloadRecord:(IxoDownloadRecord*)record;

@end