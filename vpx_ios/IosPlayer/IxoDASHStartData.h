// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import <Foundation/Foundation.h>

#import "IxoDASHManifestParser.h"

///
/// Storage class for holding DASH start data.
///
/// Start data means the initialization and index chunks. These two pieces of
/// data are required for parsing and requesting chunks.
///
@interface IxoDASHStartData : NSObject
@property(strong) IxoDASHRepresentation* representation;
@property(strong) NSData* initializationChunk;
@property(strong) NSData* indexChunk;
@property(nonatomic) NSUInteger fileLength;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithRepresentation:(IxoDASHRepresentation*)rep
    NS_DESIGNATED_INITIALIZER;

@end  // @interface IxoDASHStartData : NSObject
