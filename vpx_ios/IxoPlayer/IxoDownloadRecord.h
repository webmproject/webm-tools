// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import <Foundation/Foundation.h>

#import "IxoDataSource.h"

@interface IxoDownloadRecord : NSObject

@property(nonatomic, strong) NSURL* URL;
@property(nonatomic, strong) NSArray* requestedRange;
@property(nonatomic, strong) NSData* data;
@property(nonatomic) id<IxoDataSourceListener> listener;
@property(nonatomic) int downloadID;
@property(nonatomic) bool failed;
@property(nonatomic) NSError* error;
@property(nonatomic) IxoDataSource* dataSource;

@end
