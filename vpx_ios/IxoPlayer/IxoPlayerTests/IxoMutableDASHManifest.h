// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoDASHManifestParser.h"

//
// Mutable versions of the IxoDASHManifestParser data classes for testing.
//

@interface IxoMutableDASHRepresentation : NSObject<NSCopying>
@property(nonatomic, strong) NSString* repID;
@property(nonatomic) int bandwidth;
@property(nonatomic) int width;
@property(nonatomic) int height;
@property(nonatomic) int audioSamplingRate;
@property(nonatomic) int audioChannelConfig;
@property(nonatomic) int startWithSAP;
@property(nonatomic, strong) NSString* baseURL;
@property(nonatomic, strong) NSString* codecs;
@property(nonatomic, strong) NSArray* segmentBaseIndexRange;
@property(nonatomic, strong) NSArray* initializationRange;

- (id)init;
@end

@interface IxoMutableDASHAdaptationSet : NSObject
@property(nonatomic, strong) NSString* setID;
@property(nonatomic, strong) NSString* mimeType;
@property(nonatomic, strong) NSString* codecs;
@property(nonatomic) bool subsegmentAlignment;
@property(nonatomic) bool bitstreamSwitching;
@property(nonatomic) int subsegmentStartsWithSAP;
@property(nonatomic) int width;
@property(nonatomic) int height;
@property(nonatomic) int audioSamplingRate;
@property(nonatomic, strong)
    NSMutableArray<IxoMutableDASHRepresentation*>* representations;

- (id)init;
@end

@interface IxoMutableDASHPeriod : NSObject
@property(nonatomic, strong) NSString* periodID;
@property(nonatomic, strong) NSString* start;
@property(nonatomic, strong) NSString* duration;
@property(nonatomic, strong)
    NSMutableArray<IxoMutableDASHAdaptationSet*>* audioAdaptationSets;
@property(nonatomic, strong)
    NSMutableArray<IxoMutableDASHAdaptationSet*>* videoAdaptationSets;

- (id)init;
@end

@interface IxoMutableDASHManifest : NSObject
@property(nonatomic, strong) NSString* mediaPresentationDuration;
@property(nonatomic, strong) NSString* minBufferTime;
@property(nonatomic) bool staticPresentation;
@property(nonatomic, strong) IxoMutableDASHPeriod* period;
- (id)init;
@end
