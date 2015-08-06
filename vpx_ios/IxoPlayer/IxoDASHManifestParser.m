// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoDASHManifestParser.h"

#import "IxoDataSource.h"
#import "IxoDownloadRecord.h"

//
// IxoDASHRepresentation
//
@interface IxoDASHRepresentation()
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
@end

@implementation IxoDASHRepresentation
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
    self.bandwidth = 0;
    self.width = 0;
    self.height = 0;
    self.audioSamplingRate = 0;
    self.segmentBaseIndexRange = [[NSArray alloc] init];
    self.initializationRange = [[NSArray alloc] init];
  }
  return self;
}

@end

//
// IxoDASHAdaptationSet
//
@interface IxoDASHAdaptationSet()
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

@implementation IxoDASHAdaptationSet
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
// IxoDASHPeriod
//
@interface IxoDASHPeriod()
@property(nonatomic, strong) NSString* periodID;
@property(nonatomic, strong) NSString* start;
@property(nonatomic, strong) NSString* duration;
@property(nonatomic, strong) NSMutableArray* audioAdaptationSets;
@property(nonatomic, strong) NSMutableArray* videoAdaptationSets;
@end

@implementation IxoDASHPeriod
@synthesize periodID = _periodID;
@synthesize start = _start;
@synthesize duration = _duration;
@synthesize audioAdaptationSets = _audioAdaptationSets;
@synthesize videoAdaptationSets = _videoAdaptationSets;

- (id)init {
  self = [super init];
  if (self) {
    self.audioAdaptationSets = [[NSMutableArray alloc] init];
    self.videoAdaptationSets = [[NSMutableArray alloc] init];
  }
  return self;
}
@end

//
// IxoDASHManifest
//
@interface IxoDASHManifest()
@property(nonatomic, strong) NSString* mediaPresentationDuration;
@property(nonatomic, strong) NSString* minBufferTime;
@property(nonatomic) bool staticPresentation;
@property(nonatomic, strong) IxoDASHPeriod* period;
@end

@implementation IxoDASHManifest
@synthesize mediaPresentationDuration = _mediaPresentationDuration;
@synthesize minBufferTime = _minBufferTime;
@synthesize staticPresentation = _staticPresentation;
@synthesize period = _period;
@end

//
// IxoDASHManifestParser
//
@implementation IxoDASHManifestParser {
  NSData* _manifestData;
  NSURL* _manifestURL;
  NSLock* _lock;
  NSXMLParser* _parser;
}

- (id)init {
  self = [super init];
  if (self) {
    self = [self initWithManifestURL:nil];
  }
  return self;
}

- (instancetype)initWithManifestURL:(NSURL*)manifestURL {
  self = [super init];
  if (self) {
    _lock = [[NSLock alloc] init];
    _manifestURL = manifestURL;
  }
  return self;
}

- (bool)parse {
  if (_manifestURL == nil) {
    NSLog(@"nil _manifestURL in IxoDASHManifestParser");
    return false;
  }

  IxoDataSource* manifest_source = [[IxoDataSource alloc] init];
  _manifestData = [manifest_source downloadFromURL:_manifestURL];

  if (_manifestData == nil) {
    NSLog(@"Unable to load manifest");
    return false;
  }

  _parser = [[NSXMLParser alloc] initWithData:_manifestData];
  if (_parser == nil) {
    NSLog(@"NSXMLParser init failed");
    return false;
  }

  [_parser setDelegate:self];

  // TODO(tomfiengan): Callers will have to deal with validation of the manifest
  // contents, but a minimal check should be made that at least one
  // IxoDASHRepresenationRecord exists in one of {audio|video}AdaptationSet.
  return [_parser parse] == YES;
}

//
// NSXMLParserDelegate
//
- (void)parserDidStartDocument:(NSXMLParser*)parser {
  NSLog(@"parserDidStartDocument");
}

- (void)parser:(NSXMLParser*)parser
    didStartElement:(NSString*)elementName
       namespaceURI:(NSString*)namespaceURI
      qualifiedName:(NSString*)qName
         attributes:(NSDictionary*)attributeDict {
  NSLog(@"didStartElement --> %@ \nwith attributeDict ---> \n%@", elementName,
        attributeDict);
}

- (void)parser:(NSXMLParser*)parser foundCharacters:(NSString*)string {
  NSLog(@"foundCharacters --> %@", string);
}

- (void)parser:(NSXMLParser*)parser
 didEndElement:(NSString*)elementName
  namespaceURI:(NSString*)namespaceURI
 qualifiedName:(NSString*)qName {
  NSLog(@"didEndElement   --> %@", elementName);
}

- (void)parserDidEndDocument:(NSXMLParser*)parser {
  NSLog(@"parserDidEndDocument");
}

@end
