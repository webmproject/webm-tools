// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import <Foundation/Foundation.h>

#import "IxoDataSource.h"

///
/// Simple storage class for handling downloaded data.
///
@interface IxoDownloadRecord : NSObject

/// Download URL.
@property(nonatomic, copy) NSURL* URL;

/// Byte range to request from |URL|.
@property(nonatomic, copy) NSArray* requestedRange;

/// Data returned from |URL|.
@property(nonatomic, strong) NSData* data;

/// Listener to be notified when asynchronous download completes.
@property(nonatomic) id<IxoDataSourceListener> listener;

/// Download identifier for tracking results from asynchronous downloads.
@property(nonatomic) int downloadID;

/// Download status flag.
@property(nonatomic) bool failed;

/// Download error, if provided by underlying transport.
@property(nonatomic, strong) NSError* error;

/// IxoDataSource that owns this IxoDownloadRecord and is observing the
/// download. Non-mandatory. Notification of |listener| is performed by
/// |dataSource| when present and a listener has been provided.
@property(nonatomic) IxoDataSource* dataSource;

/// The response code returned in response to the request sent to |URL|.
@property(nonatomic) NSInteger responseCode;

/// The length of the complete resource located at |URL|. Set only for
/// ranged/partial downloads. 0 when |requestedRange| is nil.
@property(nonatomic) NSUInteger resourceLength;

/// Returns an empty record.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

@end  // @interface IxoDownloadRecord
