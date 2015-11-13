// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoDASHManifestParser.h"

#import "IxoDASHManifestParserConstants.h"
#import "IxoDataSource.h"
#import "IxoDownloadRecord.h"

//
// IxoDASHRepresentation
//
@interface IxoDASHRepresentation ()
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
@end

//
// IxoDASHAdaptationSet
//
@interface IxoDASHAdaptationSet ()
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

    if (self.representations == nil)
      return nil;
  }
  return self;
}
@end

//
// IxoDASHPeriod
//
@interface IxoDASHPeriod ()
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
    self.periodID = nil;
    self.start = nil;
    self.duration = nil;
    self.audioAdaptationSets = [[NSMutableArray alloc] init];
    self.videoAdaptationSets = [[NSMutableArray alloc] init];

    if (self.audioAdaptationSets == nil || self.videoAdaptationSets == nil)
      return nil;
  }
  return self;
}
@end

//
// IxoDASHManifest
//
@interface IxoDASHManifest ()
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

//
// IxoDASHManifestParser
//
@interface IxoDASHManifestParser ()
@property(nonatomic, strong) IxoDASHManifest* manifest;

// Sort IxoDASHRepresentation's in ascending order based on bandwidth value.
- (bool)sortRepresentationsByBandwidth;
@end

@implementation IxoDASHManifestParser {
  NSData* _manifestData;
  NSURL* _manifestURL;
  NSLock* _lock;
  NSXMLParser* _parser;
  NSString* _manifestPath;

  // Parsing status.
  bool _parseFailed;
  NSMutableArray* _openElements;
  IxoDASHAdaptationSet* _lastAdaptationSet;
}

@synthesize manifest = _manifest;

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
    if (_lock == nil)
      return nil;
    _lock.name = NSStringFromClass([self class]);

    _manifestURL = manifestURL;
    _openElements = [[NSMutableArray alloc] init];
    _parseFailed = false;
    _manifest = [[IxoDASHManifest alloc] init];

    if (_openElements == nil || _manifest == nil)
      return nil;

    // Store the manifest URL path so that baseURL props in IxoDASHRep's are
    // useful.
    _manifestPath =
        [self removeFileNameFromURLString:[manifestURL absoluteString]];
  }
  return self;
}

- (bool)parse {
  if (_manifestURL == nil) {
    NSLog(@"nil _manifestURL in IxoDASHManifestParser");
    return false;
  }

  IxoDataSource* manifest_source = [[IxoDataSource alloc] init];
  if (manifest_source == nil) {
    NSLog(@"Out of memory");
    return nil;
  }

  IxoDownloadRecord* manifest_record =
      [manifest_source downloadFromURL:_manifestURL];
  if (manifest_record == nil || manifest_record.data == nil) {
    NSLog(@"Unable to load manifest");
    return false;
  }
  _manifestData = manifest_record.data;

  _parser = [[NSXMLParser alloc] initWithData:_manifestData];
  if (_parser == nil) {
    NSLog(@"NSXMLParser init failed");
    return false;
  }

  [_parser setDelegate:self];

  if ([_parser parse] != YES) {
    NSLog(@"NSXMLParser parse failed");
    return false;
  }

  if ([self canPlayFromParsedManifest] == false) {
    NSLog(@"Parse completed but manifest data is not valid!");
    return false;
  }

  // Make sure reps in all adaptation sets are sorted by bandwidth (ascending).
  [self sortRepresentationsByBandwidth];
  return true;
}

- (NSString*)absoluteURLStringForBaseURLString:(NSString *)baseURLString {
  return [NSString stringWithFormat:@"%@%@", _manifestPath, baseURLString, nil];
}

- (bool)adaptationSetIsValid:(IxoDASHAdaptationSet*)set {
  if (set.representations.count < 1)
    return false;
  for (IxoDASHRepresentation* rep in set.representations) {
    // All representations must contain a non-nil baseURL and non-empty
    // initializationRange.
    if (rep.baseURL == nil || rep.initializationRange.count != 2)
      return false;
  }
  return true;
}

- (bool)canPlayFromParsedManifest {
  // The manifest and period must be non-nil, and at least one adaptation set
  // must be non-empty.
  if (_manifest == nil ||
      _manifest.period == nil ||
      (_manifest.period.audioAdaptationSets.count == 0 &&
          _manifest.period.videoAdaptationSets.count == 0)) {
    return false;
  }

  if (_manifest.period.audioAdaptationSets.count > 0) {
    for (IxoDASHAdaptationSet* set in _manifest.period.audioAdaptationSets) {
      if ([self adaptationSetIsValid:set] == false)
        return false;
    }
  }

  if (_manifest.period.videoAdaptationSets.count > 0) {
    for (IxoDASHAdaptationSet* set in _manifest.period.videoAdaptationSets) {
       if ([self adaptationSetIsValid:set] == false)
        return false;
    }
  }
  return true;
}

//
// Element parsing helpers.
//
// Parse utilities and parsing state sanity check helpers. All return true for
// success, and false for failure.
//
- (bool)openElementIsNamed:(NSString*)elementName {
  if ([_openElements count] < 1) {
    NSLog(@"openElementIsNamed: (%@) parse error: no open elements",
          elementName);
    return false;
  }
  if ([[_openElements lastObject] isEqualToString:elementName] == false) {
    NSLog(@"openElementIsNamed: (%@) parse error: mismatch (%@)", elementName,
          [_openElements lastObject]);
    return false;
  }
  return true;
}

- (bool)elementParentIsNamed:(NSString*)parentName {
  if ([self openElementIsNamed:parentName] == false) {
    NSLog(@"elementParentNamed: unexpected parent (%@)", parentName);
    return false;
  }
  return true;
}

- (bool)closeElementNamed:(NSString*)elementName {
  if ([self openElementIsNamed:elementName] == false) {
    NSLog(@"closeElementWithName: unexpected element (%@)", elementName);
    return false;
  }
  [_openElements removeLastObject];
  return true;
}

- (NSMutableArray*)convertStringArrayToNumberArray:(NSArray*)array {
  NSMutableArray* numbers_array = [[NSMutableArray alloc] init];
  NSNumberFormatter* formatter = [[NSNumberFormatter alloc] init];

  if (numbers_array == nil || formatter == nil)
    return nil;

  for (NSString* str in array) {
    [numbers_array addObject:[formatter numberFromString:str]];
  }
  return numbers_array;
}

// Cuts all characters from string after last occurence of '/'.
- (NSString*)removeFileNameFromURLString:(NSString*)string {
  NSRange fname_range = [string rangeOfString:@"/" options:NSBackwardsSearch];
  fname_range.length = string.length - fname_range.location;
  return [string stringByReplacingCharactersInRange:fname_range
                                         withString:@"/"];
}

//
// Attribute parsing helpers.
//
// All parse the attributes for the named element. All require a non-nil
// dictionary argument, and all attempt to ensure the parsed element has
// required attributes when applicable. All return true for success, and false
// for failure.
//
- (bool)parseMPDAttributes:(NSDictionary*)attributes {
  if (attributes == nil || [_openElements count] > 0) {
    NSLog(@"parseMPDAttributes: failure, no MPD attribs/bad state.");
    return false;
  }
  _manifest.mediaPresentationDuration =
      [attributes objectForKey:kAttributeMediaPresentationDuration];
  _manifest.minBufferTime = [attributes objectForKey:kAttributeMinBufferTime];
  _manifest.staticPresentation =
      [[attributes objectForKey:kAttributeType]
          caseInsensitiveCompare:kPresentationTypeStatic] == NSOrderedSame;
  // TODO(tomfinegan): duration restriction must be relaxed for live
  // presentations.
  if (_manifest.mediaPresentationDuration == nil ||
      _manifest.minBufferTime == nil) {
    NSLog(@"parseMPDAttributes: failure, missing required attributes.");
    return false;
  }
  return true;
}

- (bool)parsePeriodAttributes:(NSDictionary*)attributes {
  if (attributes == nil || ![self elementParentIsNamed:kElementMPD]) {
    NSLog(@"parsePeriodAttributes: failure, no attribs/bad state.");
    return false;
  }

  // TODO(tomfinegan): Support multiple periods.
  if (_manifest.period != nil) {
    NSLog(@"parsePeriodAttributes: unsupported manifest, multiple periods.");
    return false;
  }

  IxoDASHPeriod* period = [[IxoDASHPeriod alloc] init];
  if (period == nil) {
    NSLog(@"parsePeriodAttributes: out of memory.");
    return false;
  }
  period.periodID = [attributes objectForKey:kAttributeID];
  period.start = [attributes objectForKey:kAttributeStart];
  period.duration = [attributes objectForKey:kAttributeDuration];
  _manifest.period = period;
  return true;
}

- (bool)parseAdaptationSetAttributes:(NSDictionary*)attributes {
  if (attributes == nil || ![self elementParentIsNamed:kElementPeriod]) {
    NSLog(@"parseAdaptationSetAttributes: failure, no attribs/bad state.");
    return false;
  }

  IxoDASHAdaptationSet* as = [[IxoDASHAdaptationSet alloc] init];
  if (as == nil) {
    NSLog(@"parseAdaptationSetAttributes: out of memory.");
    return false;
  }

  as.setID = [attributes objectForKey:kAttributeID];
  as.mimeType = [attributes objectForKey:kAttributeMimeType];
  as.codecs = [attributes objectForKey:kAttributeCodecs];
  as.subsegmentAlignment =
      [[attributes objectForKey:kAttributeSubsegmentAlignment] boolValue] ==
      YES;
  as.bitstreamSwitching =
      [[attributes objectForKey:kAttributeBitstreamSwitching] boolValue] == YES;
  as.subsegmentStartsWithSAP =
      [[attributes objectForKey:kAttributeSubsegmentStartsWithSAP] intValue];
  as.width = [[attributes objectForKey:kAttributeWidth] intValue];
  as.height = [[attributes objectForKey:kAttributeHeight] intValue];
  as.audioSamplingRate =
      [[attributes objectForKey:kAttributeAudioSamplingRate] intValue];

  NSMutableArray* adaptation_sets = nil;
  if ([as.mimeType isEqualToString:kMimeTypeWebmAudio]) {
    adaptation_sets = _manifest.period.audioAdaptationSets;
  } else if ([as.mimeType isEqualToString:kMimeTypeWebmVideo]) {
    adaptation_sets = _manifest.period.videoAdaptationSets;
  } else {
    NSLog(@"parseAdaptationSetAttributes: unknown mimeType.");
    return false;
  }

  [adaptation_sets addObject:as];
  _lastAdaptationSet = as;
  return true;
}

- (bool)parseRepresentationAttributes:(NSDictionary*)attributes {
  if (attributes == nil || ![self elementParentIsNamed:kElementAdaptationSet]) {
    NSLog(@"parseRepresentationAttributes: failure, no attribs/bad state.");
    return false;
  }

  IxoDASHRepresentation* rep = [[IxoDASHRepresentation alloc] init];
  if (rep == nil) {
    NSLog(@"parseRepresentationAttributes: out of memory.");
    return false;
  }

  rep.repID = [attributes objectForKey:kAttributeID];
  rep.codecs = [attributes objectForKey:kAttributeCodecs];
  rep.bandwidth = [[attributes objectForKey:kAttributeBandwidth] intValue];
  rep.width = [[attributes objectForKey:kAttributeWidth] intValue];
  rep.height = [[attributes objectForKey:kAttributeHeight] intValue];
  rep.audioSamplingRate =
      [[attributes objectForKey:kAttributeAudioSamplingRate] intValue];
  rep.startWithSAP =
      [[attributes objectForKey:kAttributeStartWithSAP] intValue];

  if (rep.repID == nil) {
    NSLog(@"parseRepresentationAttributes: rep invalid, no id.");
    return false;
  }
  [_lastAdaptationSet.representations addObject:rep];
  return true;
}

- (bool)parseSegmentBaseAttributes:(NSDictionary*)attributes {
  if (attributes == nil ||
      ![self elementParentIsNamed:kElementRepresentation]) {
    NSLog(@"parseSegmentBaseAttributes: failure, no attribs/bad state.");
    return false;
  }
  // A SegmentBase must have an indexRange.
  // TODO(tomfinegan): This must be relaxed for live presentations.
  NSString* const index_range_str =
      [attributes objectForKey:kAttributeIndexRange];
  if (index_range_str == nil) {
    NSLog(@"parseSegmentBaseAttributes: missing indexRange.");
    return false;
  }
  IxoDASHRepresentation* rep = [_lastAdaptationSet.representations lastObject];
  NSArray* range_strings = [index_range_str componentsSeparatedByString:@"-"];
  rep.segmentBaseIndexRange =
      [self convertStringArrayToNumberArray:range_strings];

  if (rep.segmentBaseIndexRange.count != 2) {
    NSLog(@"parseSegmentBaseAttributes: Invalid indexRange.");
    return false;
  }
  return true;
}

- (bool)parseInitializationAttributes:(NSDictionary*)attributes {
  if (attributes == nil || ![self elementParentIsNamed:kElementSegmentBase]) {
    NSLog(@"parseInitializationAttributes: failure, no attribs/bad state.");
    return false;
  }
  // An Initialization must have a range.
  // TODO(tomfinegan): This must be relaxed for live presentations.
  NSString* const range_str = [attributes objectForKey:kAttributeRange];
  if (range_str == nil) {
    NSLog(@"parseInitializationAttributes: missing indexRange.");
    return false;
  }
  IxoDASHRepresentation* rep = [_lastAdaptationSet.representations lastObject];
  NSArray* range_strings = [range_str componentsSeparatedByString:@"-"];
  rep.initializationRange =
      [self convertStringArrayToNumberArray:range_strings];
  if (rep.initializationRange.count != 2) {
    NSLog(@"parseInitializationAttributes: Invalid range.");
    return false;
  }
  return true;
}

//
// Internal manifest management utilities.
//
- (bool)sortRepresentationsByBandwidth {
  NSSortDescriptor* sort_desc =
      [[NSSortDescriptor alloc] initWithKey:@"bandwidth" ascending:YES];
  NSArray* sort_descs = [NSArray arrayWithObjects:sort_desc, nil];

  if (sort_desc == nil || sort_descs == nil) {
    NSLog(@"Representation sort setup failed. Out of memory?");
    return false;
  }

  for (IxoDASHAdaptationSet* set in _manifest.period.audioAdaptationSets) {
    [set.representations sortUsingDescriptors:sort_descs];
  }
  for (IxoDASHAdaptationSet* set in _manifest.period.videoAdaptationSets) {
    [set.representations sortUsingDescriptors:sort_descs];
  }

  return true;
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
  NSLog(@"didStartElement (%@) \nwith attributeDict:\n%@", elementName,
        attributeDict);

  if (_parseFailed) {
    NSLog(@"didStartElement: Parsing not starting because _parseFailed.");
    [_parser abortParsing];
    return;
  }

  // Attempt to parse attributes for elements that normally include them.
  if ([elementName isEqualToString:kElementMPD]) {
    _parseFailed = [self parseMPDAttributes:attributeDict] != true;
  } else if ([elementName isEqualToString:kElementPeriod]) {
    _parseFailed = [self parsePeriodAttributes:attributeDict] != true;
  } else if ([elementName isEqualToString:kElementAdaptationSet]) {
    _parseFailed = [self parseAdaptationSetAttributes:attributeDict] != true;
  } else if ([elementName isEqualToString:kElementRepresentation]) {
    _parseFailed = [self parseRepresentationAttributes:attributeDict] != true;
  } else if ([elementName isEqualToString:kElementAudioChannelConfiguration]) {
    if ([_lastAdaptationSet.mimeType isEqualToString:kMimeTypeWebmAudio]) {
      IxoDASHRepresentation* rep =
          [_lastAdaptationSet.representations lastObject];
      rep.audioChannelConfig =
          [[attributeDict objectForKey:kAttributeValue] intValue];
    } else {
      NSLog(
          @"didStartElement: _parseFailed, non-audio rep with channel config");
      _parseFailed = true;
    }
  } else if ([elementName isEqualToString:kElementSegmentBase]) {
    _parseFailed = [self parseSegmentBaseAttributes:attributeDict] != true;
  } else if ([elementName isEqualToString:kElementInitialization]) {
    _parseFailed = [self parseInitializationAttributes:attributeDict] != true;
  }

  if (_parseFailed) {
    NSLog(@"didStartElement: Parsing stopped because _parseFailed.");
    [_parser abortParsing];
  }

  [_openElements addObject:elementName];
}

- (void)parser:(NSXMLParser*)parser foundCharacters:(NSString*)string {
  NSLog(@"foundCharacters (%@)", string);
  if (_parseFailed) {
    NSLog(@"Parsing stopped because _parseFailed in foundCharacters.");
    [_parser abortParsing];
    return;
  }

  if ([[_openElements lastObject] isEqualToString:kElementBaseURL]) {
    IxoDASHRepresentation* rep =
        [_lastAdaptationSet.representations lastObject];
    rep.baseURL = string;
  }
}

- (void)parser:(NSXMLParser*)parser
 didEndElement:(NSString*)elementName
  namespaceURI:(NSString*)namespaceURI
 qualifiedName:(NSString*)qName {
  NSLog(@"didEndElement (%@)", elementName);

  if (_parseFailed) {
    NSLog(@"Parsing stopped because _parseFailed in didEndElement.");
    [_parser abortParsing];
    return;
  }

  _parseFailed = [self closeElementNamed:elementName] != true;

  if (_parseFailed) {
    NSLog(@"didEndElement set _parseFailed on element (%@)", elementName);
  }
}

- (void)parserDidEndDocument:(NSXMLParser*)parser {
  _lastAdaptationSet = nil;
  NSLog(@"parserDidEndDocument");
}

@end
