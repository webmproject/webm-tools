// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoManifestParser.h"

#import "IxoDataSource.h"
#import "IxoDownloadRecord.h"

@implementation IxoManifestParser {
  NSData* _manifestData;
  NSURL* _manifestURL;
  NSLock* _lock;
  NSXMLParser* _parser;
}

- (id)init {
  self = [super init];
  _manifestURL = nil;
  _lock = [[NSLock alloc] init];
  return self;
}

- (instancetype)initWithManifestURL:(NSURL*)manifestURL {
  self = [IxoManifestParser init];
  _manifestURL = manifestURL;
  return self;
}

- (bool)parse {
  if (_manifestURL == nil) {
    NSLog(@"nil _manifestURL in IxoManifestParser");
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
