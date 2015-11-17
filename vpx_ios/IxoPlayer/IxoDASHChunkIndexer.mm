// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#import "IxoDASHChunkIndexer.h"

#include <cstdint>
#include <memory>

#include "mkvparser.hpp"
#include "webm_incremental_reader.h"

@implementation IxoDASHChunkIndex
@synthesize chunkRanges = _chunkRanges;

- (instancetype)init {
  self = [super init];
  if (self) {
    self.chunkRanges = [[NSMutableArray alloc] init];
  }
  return self;
}
@end  // @implementation IxoDASHChunkIndex

@implementation IxoDASHChunkIndexer {
  NSMutableData* _webm_buffer;
  std::unique_ptr<mkvparser::Segment> _webm_parser;
  webm_tools::WebmIncrementalReader _webm_reader;
  NSUInteger _file_length;
}

- (instancetype)initWithDASHStartData:(IxoDASHStartData*)startData {
  self = [super init];
  if (self != nil) {
    _file_length = startData.fileLength;
    _webm_buffer = [NSMutableData dataWithData:startData.initializationChunk];
    if (_webm_buffer == nil) {
      NSLog(@"Out of memory.");
      return nil;
    }
    [_webm_buffer appendData:startData.indexChunk];

    std::int64_t status = _webm_reader.SetBufferWindow(
        reinterpret_cast<const uint8_t*>(_webm_buffer.bytes),
        static_cast<int32_t>(_webm_buffer.length), 0 /* bytes_consumed */);
    if (status != webm_tools::WebmIncrementalReader::kSuccess) {
      NSLog(@"cannot set incremental reader buffer window");
      return nil;
    }

    if (_webm_reader.SetEndOfSegmentPosition(_webm_buffer.length) != true) {
      NSLog(@"cannot set incremental reader segment end position.");
      return nil;
    }

    using mkvparser::Segment;
    Segment* segment = nullptr;
    if ((status = Segment::CreateInstance(&_webm_reader, 0 /* pos */,
                                          segment /* Segment*& */)) != 0 ||
        segment == nullptr) {
      NSLog(@"Parser Segment creation failed: %lld", status);
      return nil;
    }
    _webm_parser.reset(segment);
  }
  return self;
}

- (std::int64_t)clusterPosFromCuePoint:(const mkvparser::CuePoint*)cue
                              andTrack:(const mkvparser::Track*)track {
  std::int64_t pos = 0;
  if (cue != nullptr && track != nullptr) {
    using mkvparser::CuePoint;
    const CuePoint::TrackPosition* const track_pos = cue->Find(track);
    if (track_pos == nullptr) {
      NSLog(@"Chunk indexer can't find cluster position.");
    } else {
      pos = track_pos->m_pos;
    }
  }
  return pos;
}

- (IxoDASHChunkIndex*)buildChunkIndex {
  if (_webm_buffer == nil || _webm_parser.get() == nullptr) {
    NSLog(@"chunk indexer not initialized.");
    return nil;
  }

  IxoDASHChunkIndex* index = [[IxoDASHChunkIndex alloc] init];
  if (index == nil) {
    NSLog(@"Out of memory in chunk indexer.");
    return nil;
  }

  // Start the parse (mkvparser parses init chunk and inspects index chunk).
  const std::int64_t status = _webm_parser->Load();
  if (status != 0) {
    NSLog(@"Chunk indexer failed during parse.");
    return nil;
  }

  const mkvparser::Cues* const cues = _webm_parser->GetCues();
  if (cues == nullptr) {
    NSLog(@"Chunk indexer found no cues.");
    return nil;
  }

  // Parse the index chunk (aka walk/parse cues).
  // TODO(tomfinegan): Parse only the first, walk later? This might take too
  // for a very long presentation.
  while (cues->DoneParsing() == false)
    cues->LoadCuePoint();

  const NSInteger num_cue_points = cues->GetCount();
  const mkvparser::CuePoint* cue_point = cues->GetFirst();
  if (cue_point == nullptr || num_cue_points == 0) {
    NSLog(@"Chunk indexer found no cues after successful parse");
    return nil;
  }
  const mkvparser::Track* const track =
      _webm_parser->GetTracks()->GetTrackByIndex(0);
  if (track == nullptr) {
    NSLog(@"Chunk indexer can't find track 0.");
    return nil;
  }

  for (;;) {
    const std::int64_t cluster_start_pos =
        [self clusterPosFromCuePoint:cue_point andTrack:track];

    if (cluster_start_pos == 0) {
      NSLog(@"Chunk indexer can't find start pos.");
      return nil;
    }

    cue_point = cues->GetNext(cue_point);

    std::int64_t cluster_end_pos =
        cue_point == nullptr
            ? _file_length
            : [self clusterPosFromCuePoint:cue_point andTrack:track];

    NSArray* const range_array = [NSArray
        arrayWithObjects:[NSNumber numberWithUnsignedInteger:cluster_start_pos],
                         [NSNumber numberWithUnsignedInteger:cluster_end_pos],
                         nil];
    [index.chunkRanges addObject:range_array];

    if (cue_point == nullptr)
      break;
  }
  return index;
}

@end  // @implementation IxoDASHChunkIndexer
