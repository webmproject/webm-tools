// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoDASHStartData.h"

@implementation IxoDASHStartData

@synthesize initializationChunk;
@synthesize indexChunk;
@synthesize fileLength = _fileLength;

- (id)initWithRepresentation:(IxoDASHRepresentation *)rep {
  if (rep == nil) {
    NSLog(@"Can't init rep record with nil representation.");
    return nil;
  }

  self = [super init];
  if (self == nil) {
    NSLog(@"Out of memory");
    return self;
  }

  self.representation = rep;
  self.initializationChunk = nil;
  self.indexChunk = nil;
  self.fileLength = 0;
  return self;
}

@end  // @implementation IxoDASHStartData
