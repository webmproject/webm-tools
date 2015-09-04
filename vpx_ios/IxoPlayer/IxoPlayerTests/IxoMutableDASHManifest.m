// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoMutableDASHManifest.h"

//
// IxoMutableDASHRepresentation
//
@implementation IxoMutableDASHRepresentation
@synthesize repID = _repID;
@synthesize bandwidth = _bandwidth;
@synthesize width = _width;
@synthesize height = _height;
@synthesize audioSamplingRate = _audioSamplingRate;
@synthesize audioChannelConfig = _audioChannelConfig;
@synthesize startWithSAP = _startWithSAP;
@synthesize codecs = _codecs;
@synthesize baseURL = _baseURL;
@synthesize segmentBaseIndexRange = _segmentBaseIndexRange;
@synthesize initializationRange = _initializationRange;

- (id)init {
  self = [super init];
  if (self) {
    self.repID = nil;
    self.bandwidth = 0;
    self.width = 0;
    self.height = 0;
    self.audioSamplingRate = 0;
    self.audioChannelConfig = 0;
    self.startWithSAP = 0;
    self.codecs = nil;
    self.baseURL = nil;
    self.segmentBaseIndexRange = nil;
    self.initializationRange = nil;
  }
  return self;
}

- (id)copyWithZone:(NSZone*)zone {
  IxoMutableDASHRepresentation* copy =
      [[IxoMutableDASHRepresentation alloc] init];
  if (copy) {
    copy.repID = self.repID;
    copy.bandwidth = self.bandwidth;
    copy.width = self.width;
    copy.height = self.height;
    copy.audioSamplingRate = self.audioSamplingRate;
    copy.audioChannelConfig = self.audioChannelConfig;
    copy.startWithSAP = self.startWithSAP;
    copy.codecs = self.codecs;
    copy.baseURL = self.baseURL;
    copy.segmentBaseIndexRange = self.segmentBaseIndexRange;
    copy.initializationRange = self.initializationRange;
  }
  return copy;
}
@end

//
// IxoMutableDASHAdaptationSet
//
@implementation IxoMutableDASHAdaptationSet
@synthesize setID = _setID;
@synthesize mimeType = _mimeType;
@synthesize codecs = _codecs;
@synthesize subsegmentAlignment = _subsegmentAlignment;
@synthesize bitstreamSwitching = _bitstreamSwitching;
@synthesize subsegmentStartsWithSAP = _subsegmentStartsWithSAP;
@synthesize width = _width;
@synthesize height = _height;
@synthesize audioSamplingRate = _audioSamplingRate;
@synthesize representations = _representations;

- (id)init {
  self = [super init];
  if (self) {
    self.setID = nil;
    self.mimeType = nil;
    self.codecs = nil;
    self.subsegmentAlignment = false;
    self.bitstreamSwitching = false;
    self.subsegmentStartsWithSAP = 0;
    self.width = 0;
    self.height = 0;
    self.audioSamplingRate = 0;
    self.representations = [[NSMutableArray alloc] init];
  }
  return self;
}
@end

//
// IxoMutableDASHPeriod
//
@implementation IxoMutableDASHPeriod
@synthesize periodID = _periodID;
@synthesize start = _start;
@synthesize duration = _duration;
@synthesize audioAdaptationSets = _audioAdaptationSets;
@synthesize videoAdaptationSets = _videoAdaptationSets;

- (id)init {
  self = [super init];
  if (self) {
    self.periodID = nil;
    self.start = nil;
    self.duration = nil;
    self.audioAdaptationSets = [[NSMutableArray alloc] init];
    self.videoAdaptationSets = [[NSMutableArray alloc] init];
  }
  return self;
}
@end

//
// IxoMutableDASHManifest
//
@implementation IxoMutableDASHManifest
@synthesize mediaPresentationDuration = _mediaPresentationDuration;
@synthesize minBufferTime = _minBufferTime;
@synthesize staticPresentation = _staticPresentation;
@synthesize period = _period;

- (id)init {
  self = [super init];
  if (self) {
    self.mediaPresentationDuration = nil;
    self.minBufferTime = nil;
    self.staticPresentation = false;
    self.period = nil;
  }
  return self;
}
@end