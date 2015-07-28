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
// IxoDASHRepresentationRecord
//
@implementation IxoDASHRepresentationRecord
@synthesize repID = _repID;
@synthesize bandwidth = _bandwidth;
@synthesize width = _width;
@synthesize height = _height;
@synthesize baseURL = _baseURL;
@synthesize segmentBase = _segmentBase;
@synthesize initialization = _initialization;
@end

//
// IxoDASHRepresentationStore
//
@implementation IxoDASHRepresentationStore
@synthesize  representationRecords = _representationRecords;
@end

//
// IxoDASHAdaptationSet
//
@implementation IxoDASHAdaptationSet
@synthesize mimeType = _mimeType;
@synthesize codecs = _codecs;
@synthesize subsegmentAlignment = _subsegmentAlignment;
@synthesize bitstreamSwitching = _bitstreamSwitching;
@synthesize width = _width;
@synthesize height = _height;
@end

//
// IxoDASHPeriod
//
@implementation IxoDASHPeriod
@synthesize periodID = _periodID;
@synthesize start = _start;
@synthesize duration = _duration;
@end


//
// IxoDASHManifest
//
@implementation IxoDASHManifest
@synthesize mediaPresentationDuration = _mediaPresentationDuration;
@synthesize minBufferTime = _minBufferTime;
@synthesize staticPresentation = _staticPresentation;
@synthesize audioAdaptationSets = _audioAdaptationSets;
@synthesize videoAdaptationSets = _videoAdaptationSets;
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
