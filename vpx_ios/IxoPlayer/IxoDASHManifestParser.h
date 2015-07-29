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
@property(nonatomic, strong) NSString* repID;
@property(nonatomic) int bandwidth;
@property(nonatomic) int width;
@property(nonatomic) int height;
@property(nonatomic) int audioSamplingRate;
@property(nonatomic) int audioChannelConfig;
@property(nonatomic) int startWithSAP;
@property(nonatomic, strong) NSString* baseURL;
@property(nonatomic, strong) NSString* codecs;
@property(nonatomic, strong) NSArray* segmentBase;
@property(nonatomic, strong) NSArray* initialization;
@end

@interface IxoDASHAdaptationSet : NSObject
@property(nonatomic, strong) NSString* setID;
@property(nonatomic, strong) NSString* mimeType;
@property(nonatomic, strong) NSString* codecs;
@property(nonatomic) bool subsegmentAlignment;
@property(nonatomic) bool bitstreamSwitching;
@property(nonatomic) int subsegmentStartsWithSAP;
@property(nonatomic) int width;
@property(nonatomic) int height;
@property(nonatomic) int audioSamplingRate;
@property(nonatomic, strong) NSMutableArray* representations;
@end

@interface IxoDASHPeriod : NSObject
@property(nonatomic, strong) NSString* periodID;
@property(nonatomic, strong) NSString* start;
@property(nonatomic, strong) NSString* duration;
@property(nonatomic, strong) NSMutableArray* audioAdaptationSets;
@property(nonatomic, strong) NSMutableArray* videoAdaptationSets;
@end

@interface IxoDASHManifest : NSObject
@property(nonatomic, strong) NSString* mediaPresentationDuration;
@property(nonatomic, strong) NSString* minBufferTime;
@property(nonatomic) bool staticPresentation;
@property(nonatomic, strong) IxoDASHPeriod* period;
@end

@interface IxoDASHManifestParser : NSObject<NSXMLParserDelegate>
- (instancetype)initWithManifestURL:(NSURL*)manifestURL;
- (bool)parse;
@end
