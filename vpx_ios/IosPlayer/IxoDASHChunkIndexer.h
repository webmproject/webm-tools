// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#import <Foundation/Foundation.h>

#import "IxoDASHStartData.h"

///
/// Wrapper class. Stores typed mutable array of arrays. Outer array is chunk
/// index. Inner array is the chunk offsets for use in ranged HTTP requests.
///
@interface IxoDASHChunkIndex : NSObject
@property(nonatomic, strong) NSMutableArray<NSArray*>* chunkRanges;
- (instancetype)init NS_DESIGNATED_INITIALIZER;
@end  // @interface IxoDASHChunkIndex : NSObject

///
/// Parses the init and index chunks stored within an IxoDASHStartData to yield
/// an array of chunk offset and length arrays.
///
@interface IxoDASHChunkIndexer : NSObject

/// Inits indexer for building chunk indexes.
- (instancetype)initWithDASHStartData:(IxoDASHStartData*)startData
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

/// Builds the chunk index and returns it. Returns nil upon failure.
- (IxoDASHChunkIndex*)buildChunkIndex;

@end  // @interface IxoDASHChunkIndexer : NSObject
