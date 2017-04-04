// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#import "IxoDASHChunkIndexer.h"

#import <XCTest/XCTest.h>

#import "IxoDataSource.h"
#import "IxoDASHManifestParser.h"
#import "IxoDASHStartData.h"
#import "IxoDownloadRecord.h"
#import "IosPlayerTestCommon.h"

@interface IxoDASHChunkIndexerTests : XCTestCase {
  IxoDataSource* _data_source;
  IxoDASHManifestParser* _parser;
  NSDictionary<NSString*, NSArray*>* _expected_chunk_index_arrays;
}
@end  // @interface IxoDASHChunkIndexerTests : XCTestCase

@implementation IxoDASHChunkIndexerTests

- (void)setUp {
  [super setUp];
  _data_source = [[IxoDataSource alloc] init];
  XCTAssertNotNil(_data_source);

  _parser = [[IxoDASHManifestParser alloc]
      initWithManifestURL:
          [NSURL URLWithString:kVP9VorbisRepCodecsDASHMPD1URLString]];
  XCTAssertNotNil(_parser);
  XCTAssert([_parser parse] == true);

  // Cluster indexes produced via running testdata/print_chunk_pos_array.sh
  // on WebM files from manifest at |kVP9VorbisRepCodecsDASHMPD1URLString|.
  NSArray<NSArray*>* const rep242_chunk_index = [[NSArray alloc]
      initWithObjects:@[ @635, @111193 ], @[ @111193, @241139 ],
                      @[ @241139, @377640 ], @[ @377640, @493056 ],
                      @[ @493056, @611391 ], @[ @611391, @861207 ],
                      @[ @861207, @1065736 ], @[ @1065736, @1372423 ],
                      @[ @1372423, @1528545 ], @[ @1528545, @1673187 ],
                      @[ @1673187, @1788971 ], @[ @1788971, @1928651 ],
                      @[ @1928651, @2108817 ], @[ @2108817, @2305183 ],
                      @[ @2305183, @2585273 ], @[ @2585273, @2854060 ],
                      @[ @2854060, @3051069 ], @[ @3051069, @3252663 ],
                      @[ @3252663, @3456384 ], @[ @3456384, @3636515 ],
                      @[ @3636515, @3826395 ], @[ @3826395, @4017132 ],
                      @[ @4017132, @4135028 ], @[ @4135028, @4249859 ],
                      @[ @4249859, @4360399 ], @[ @4360399, @4459931 ],
                      @[ @4459931, @4478156 ], nil];
  NSArray<NSArray*>* const rep243_chunk_index = [[NSArray alloc]
      initWithObjects:@[ @636, @201610 ], @[ @201610, @424298 ],
                      @[ @424298, @661937 ], @[ @661937, @880896 ],
                      @[ @880896, @1101714 ], @[ @1101714, @1568058 ],
                      @[ @1568058, @1928728 ], @[ @1928728, @2508821 ],
                      @[ @2508821, @2785115 ], @[ @2785115, @3019287 ],
                      @[ @3019287, @3237720 ], @[ @3237720, @3471877 ],
                      @[ @3471877, @3789793 ], @[ @3789793, @4113437 ],
                      @[ @4113437, @4611518 ], @[ @4611518, @5088086 ],
                      @[ @5088086, @5399330 ], @[ @5399330, @5739461 ],
                      @[ @5739461, @6059188 ], @[ @6059188, @6376457 ],
                      @[ @6376457, @6699461 ], @[ @6699461, @7044041 ],
                      @[ @7044041, @7262049 ], @[ @7262049, @7474615 ],
                      @[ @7474615, @7682021 ], @[ @7682021, @7874020 ],
                      @[ @7874020, @7902885 ], nil];
  NSArray<NSArray*>* const rep244_chunk_index = [[NSArray alloc]
      initWithObjects:@[ @636, @410213 ], @[ @410213, @843155 ],
                      @[ @843155, @1286235 ], @[ @1286235, @1721398 ],
                      @[ @1721398, @2143058 ], @[ @2143058, @2845663 ],
                      @[ @2845663, @3384080 ], @[ @3384080, @4268526 ],
                      @[ @4268526, @4725222 ], @[ @4725222, @5156191 ],
                      @[ @5156191, @5587411 ], @[ @5587411, @6021609 ],
                      @[ @6021609, @6515380 ], @[ @6515380, @6991717 ],
                      @[ @6991717, @7728598 ], @[ @7728598, @8438184 ],
                      @[ @8438184, @8882561 ], @[ @8882561, @9367367 ],
                      @[ @9367367, @9832677 ], @[ @9832677, @10307336 ],
                      @[ @10307336, @10783612 ], @[ @10783612, @11292496 ],
                      @[ @11292496, @11725301 ], @[ @11725301, @12140024 ],
                      @[ @12140024, @12557703 ], @[ @12557703, @12898076 ],
                      @[ @12898076, @12941793 ], nil];
  NSArray<NSArray*>* const rep245_chunk_index = [[NSArray alloc]
      initWithObjects:@[ @637, @627772 ], @[ @627772, @1280766 ],
                      @[ @1280766, @1943185 ], @[ @1943185, @2593177 ],
                      @[ @2593177, @3230595 ], @[ @3230595, @3977092 ],
                      @[ @3977092, @4632036 ], @[ @4632036, @5527618 ],
                      @[ @5527618, @6180361 ], @[ @6180361, @6822834 ],
                      @[ @6822834, @7458469 ], @[ @7458469, @8103687 ],
                      @[ @8103687, @8758500 ], @[ @8758500, @9408759 ],
                      @[ @9408759, @10175229 ], @[ @10175229, @10892295 ],
                      @[ @10892295, @11545125 ], @[ @11545125, @12197804 ],
                      @[ @12197804, @12844512 ], @[ @12844512, @13494013 ],
                      @[ @13494013, @14131419 ], @[ @14131419, @14778963 ],
                      @[ @14778963, @15426828 ], @[ @15426828, @16055646 ],
                      @[ @16055646, @16693179 ], @[ @16693179, @17174322 ],
                      @[ @17174322, @17220192 ], nil];
  NSArray<NSArray*>* const rep246_chunk_index = [[NSArray alloc]
      initWithObjects:@[ @649, @1180563 ], @[ @1180563, @2468248 ],
                      @[ @2468248, @3776504 ], @[ @3776504, @5060728 ],
                      @[ @5060728, @6344619 ], @[ @6344619, @7643149 ],
                      @[ @7643149, @8936838 ], @[ @8936838, @10251447 ],
                      @[ @10251447, @11531209 ], @[ @11531209, @12788134 ],
                      @[ @12788134, @14060424 ], @[ @14060424, @15337023 ],
                      @[ @15337023, @16644570 ], @[ @16644570, @17943480 ],
                      @[ @17943480, @19215525 ], @[ @19215525, @20519371 ],
                      @[ @20519371, @21810979 ], @[ @21810979, @23109195 ],
                      @[ @23109195, @24402937 ], @[ @24402937, @25686086 ],
                      @[ @25686086, @26977073 ], @[ @26977073, @28264277 ],
                      @[ @28264277, @29550524 ], @[ @29550524, @30789176 ],
                      @[ @30789176, @31914665 ], @[ @31914665, @32556320 ],
                      @[ @32556320, @32602936 ], nil];
  NSArray<NSArray*>* const rep247_chunk_index = [[NSArray alloc]
      initWithObjects:@[ @648, @829293 ], @[ @829293, @1705373 ],
                      @[ @1705373, @2590404 ], @[ @2590404, @3464205 ],
                      @[ @3464205, @4319358 ], @[ @4319358, @5841293 ],
                      @[ @5841293, @6995877 ], @[ @6995877, @9445324 ],
                      @[ @9445324, @10424469 ], @[ @10424469, @11295453 ],
                      @[ @11295453, @12181427 ], @[ @12181427, @13048428 ],
                      @[ @13048428, @14079793 ], @[ @14079793, @15144203 ],
                      @[ @15144203, @16814508 ], @[ @16814508, @18443728 ],
                      @[ @18443728, @19336096 ], @[ @19336096, @20320931 ],
                      @[ @20320931, @21225279 ], @[ @21225279, @22226523 ],
                      @[ @22226523, @23212150 ], @[ @23212150, @24369373 ],
                      @[ @24369373, @25238129 ], @[ @25238129, @26074903 ],
                      @[ @26074903, @26921812 ], @[ @26921812, @27670764 ],
                      @[ @27670764, @27757852 ], nil];
  _expected_chunk_index_arrays = [NSDictionary
      dictionaryWithObjectsAndKeys:rep242_chunk_index, @"242",
                                   rep243_chunk_index, @"243",
                                   rep244_chunk_index, @"244",
                                   rep245_chunk_index, @"245",
                                   rep246_chunk_index, @"246",
                                   rep247_chunk_index, @"247", nil];
}

- (void)tearDown {
  [super tearDown];
}


// Returns results of HTTP download.
- (IxoDownloadRecord*)fetchChunkAtURL:(NSURL*)url withRange:(NSArray*)range {
  return [_data_source downloadFromURL:url withRange:range];
}

// Creates IxoDASHStartData for the given IxoDASHRepresentation.
- (IxoDASHStartData*)fetchStartDataForRep:(IxoDASHRepresentation*)rep {
  IxoDASHStartData* const start_data =
      [[IxoDASHStartData alloc] initWithRepresentation:rep];
  XCTAssertNotNil(start_data);

  NSURL* const chunk_url = [NSURL
      URLWithString:[_parser absoluteURLStringForBaseURLString:rep.baseURL]];
  XCTAssertNotNil(chunk_url);

  IxoDownloadRecord* record =
      [self fetchChunkAtURL:chunk_url withRange:rep.initializationRange];
  start_data.initializationChunk = record.data;
  XCTAssertNotNil(start_data.initializationChunk);

  record = [self fetchChunkAtURL:chunk_url withRange:rep.segmentBaseIndexRange];
  start_data.indexChunk = record.data;
  XCTAssertNotNil(start_data.indexChunk);

  start_data.fileLength = record.resourceLength;
  XCTAssert(start_data.fileLength > 0);

  return start_data;
}

- (void)testChunkIndexing {
  for (IxoDASHRepresentation* rep in
       [_parser videoRepsForAdaptationSetWithIndex:0]) {
    IxoDASHStartData* const start_data = [self fetchStartDataForRep:rep];
    XCTAssertNotNil(start_data);

    IxoDASHChunkIndexer* const indexer =
        [[IxoDASHChunkIndexer alloc] initWithDASHStartData:start_data];
    XCTAssertNotNil(indexer);

    IxoDASHChunkIndex* const index = [indexer buildChunkIndex];
    XCTAssertNotNil(index);

    NSArray* const expected_chunk_ranges =
        [_expected_chunk_index_arrays objectForKey:rep.repID];
    XCTAssert([index.chunkRanges isEqualToArray:expected_chunk_ranges]);
  }
}

@end  // @implementation IxoDASHChunkIndexerTests
