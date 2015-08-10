// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import <Foundation/Foundation.h>

#import "IxoDataSource.h"

@interface IxoDASHRepresentation : NSObject
@property(nonatomic, strong, readonly) NSString* repID;
@property(nonatomic, readonly) int bandwidth;
@property(nonatomic, readonly) int width;
@property(nonatomic, readonly) int height;
@property(nonatomic, readonly) int audioSamplingRate;
@property(nonatomic, readonly) int audioChannelConfig;
@property(nonatomic, readonly) int startWithSAP;
@property(nonatomic, strong, readonly) NSString* baseURL;
@property(nonatomic, strong, readonly) NSString* codecs;
@property(nonatomic, strong, readonly) NSArray* segmentBaseIndexRange;
@property(nonatomic, strong, readonly) NSArray* initializationRange;
@end

@interface IxoDASHAdaptationSet : NSObject
@property(nonatomic, strong, readonly) NSString* setID;
@property(nonatomic, strong, readonly) NSString* mimeType;
@property(nonatomic, strong, readonly) NSString* codecs;
@property(nonatomic, readonly) bool subsegmentAlignment;
@property(nonatomic, readonly) bool bitstreamSwitching;
@property(nonatomic, readonly) int subsegmentStartsWithSAP;
@property(nonatomic, readonly) int width;
@property(nonatomic, readonly) int height;
@property(nonatomic, readonly) int audioSamplingRate;
@property(nonatomic, strong, readonly) NSMutableArray* representations;
@end

@interface IxoDASHPeriod : NSObject
@property(nonatomic, strong, readonly) NSString* periodID;
@property(nonatomic, strong, readonly) NSString* start;
@property(nonatomic, strong, readonly) NSString* duration;
@property(nonatomic, strong, readonly) NSMutableArray* audioAdaptationSets;
@property(nonatomic, strong, readonly) NSMutableArray* videoAdaptationSets;
@end

@interface IxoDASHManifest : NSObject
@property(nonatomic, strong, readonly) NSString* mediaPresentationDuration;
@property(nonatomic, strong, readonly) NSString* minBufferTime;
@property(nonatomic, readonly) bool staticPresentation;
@property(nonatomic, strong, readonly) IxoDASHPeriod* period;
@end

@interface IxoDASHManifestParser : NSObject<NSXMLParserDelegate>
@property(nonatomic, strong, readonly) IxoDASHManifest* manifest;
- (instancetype)initWithManifestURL:(NSURL*)manifestURL;
- (bool)parse;
@end
