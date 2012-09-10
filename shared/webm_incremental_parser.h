/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SHARED_WEBM_INCREMENTAL_PARSER_H_
#define SHARED_WEBM_INCREMENTAL_PARSER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webm_tools_types.h"

namespace mkvparser {
class AudioTrack;
class Block;
class Cluster;
class CuePoint;
class Cues;
class MkvReader;
class Segment;
class SegmentInfo;
class Track;
class VideoTrack;
}  // namespace mkvparser

namespace webm_tools {

class WebmIncrementalReader;

// This class is used to parse a WebM file incrementally using libwebm. The
// class adds convenience functions to gather information about WebM files.
// The code using WebMIncrementalParser must pay attention to the state of
// WebMIncrementalParser as the values returned may change over time as more
// data is parsed. Most of the functions return a Status which is current
// state of the parser. The state starts in kParsingHeaders. After the parser
// has parsed all of the header data the state will transition to
// kParsingClusters. After the parser has finished parsing all of the cluster
// the state will transition to kDoneParsing. Check the functions comments as
// to what state the parser needs to be in to output valid values.
//
// After the WebMIncrementalParser is created the calling code must feed data
// into WebMIncrementalParser by calling ParseNextChunk. If a level 1 WebM
// element is parsed the size of the element is passed back. The calling code
// is responsible for removing the data of the element that was just parsed.
class WebMIncrementalParser {
 public:
  typedef std::vector<uint8> Buffer;

  enum TrackTypes {
    kUnknown = 0,
    kVideo = 1,
    kAudio = 2,
  };

  enum Status {
    kInvalidWebM = -2,
    kParsingError = -1,
    kParsingHeader = 1,
    kParsingClusters = 2,
    kParsingFinalElements = 3,
    kDoneParsing = 4,
  };

  WebMIncrementalParser();
  ~WebMIncrementalParser();

  // Creates the incremental reader.
  bool Init();

  // |value| returns true if the file contains at least one audio track. Parser
  // state must be >= kParsingClusters for output to be valid.
  Status HasAudio(bool* value) const;

  // |num_channels| returns the number of channels of the first audio track.
  // Returns 0 if there is no audio track. Parser state must >=
  // kParsingClusters for output to be valid.
  Status AudioChannels(int* num_channels) const;

  // |sample_rate| returns the sample rate of the first audio track. Returns 0
  // if there is no audio track. Parser state must be >= kParsingClusters for
  // output to be valid.
  Status AudioSampleRate(int* sample_rate) const;

  // Returns the audio sample size in bits per sample of the first audio track.
  // Returns 0 if there no audio track. Parser state must be >=
  // kParsingClusters for output to be valid.
  Status AudioSampleSize(int* sample_size) const;

  // Returns the average framerate of the first video track. Returns 0.0 if
  // there is no video track or we cannot calculate an average framerate.
  // Parser state must equal kDoneParsing for output to be valid.
  Status CalculateVideoFrameRate(double* framerate) const;

  // Returns true if the file has a Cues element. Parser state must equal
  // kDoneParsing for output to be valid.
  Status CheckForCues(bool* value) const;

  // Returns true if the first Block of every CuePoint is the first Block in
  // the Cluster for that track. Parser state must equal kDoneParsing for
  // output to be valid.
  Status CuesFirstInCluster(TrackTypes type, bool* value) const;

  // Calculate and returns average bits per second for the WebM file. Parser
  // state must be >= kParsingClusters and the end position must be set for
  // output to be valid.
  Status FileAverageBitsPerSecond(int64* average_bps) const;

  // Returns the length of the file in bytes. Parser state must be >=
  // kParsingClusters and the end position must be set for output to be valid.
  Status FileLength(int64* length) const;

  // Calculates and returns maximum bits per second for the WebM file. Parser
  // state must equal kDoneParsing for output to be valid.
  Status FileMaximumBitsPerSecond(int64* maximum_bps) const;

  // Returns the codec string associated with the file. If the CodecID
  // is V_VP8 then the string returned will be "vp8". If the CodecID is
  // A_VORBIS then the string returned will be "vorbis". If there is more
  // than one track in the file the codecs will be in a comma separated list
  // like "vp8, vorbis". If the CodecID is anything else then the string
  // returned will be empty. Parser state must be >= kParsingClusters for
  // output to be valid.
  Status GetCodec(std::string* codec) const;

  // Returns the Cues from the webm file. Parser state must equal kDoneParsing
  // for output to be valid.
  Status GetCues(const mkvparser::Cues** cues) const;

  // Returns the duration of the file in nanoseconds. Parser state must be >=
  // kParsingClusters for output to be valid.
  Status GetDurationNanoseconds(int64* duration) const;

  // Returns the mimetype string associated with the file. Returns
  // "video/webm" if the file is a valid WebM file. Returns the empty string
  // if not a valid WebM file. Parser state must be >= kParsingClusters for
  // output to be valid.
  Status GetMimeType(std::string* mimetype) const;

  // Returns the mimetype with the codec parameter for the first two tracks
  // in the file. The format is defined by the WebM specification. Returns the
  // empty string if not a valid WebM file. Parser state must be >=
  // kParsingClusters for output to be valid.
  Status GetMimeTypeWithCodec(std::string* mimetype_with_codec) const;

  // Returns the SegmentInfo element. Parser state must be >= kParsingClusters
  // for output to be valid.
  Status GetSegmentInfo(const mkvparser::SegmentInfo** info) const;

  // Returns the starting byte offset for the Segment element. Parser state must
  // be >= kParsingClusters for output to be valid.
  Status GetSegmentStartOffset(int64* offset) const;

  // Returns true if the first video track equals V_VP8 or the first audio
  // track equals A_VORBIS. Returns false if there are no audio or video
  // tracks. Returns false if there is both a video tack and an audio track.
  // Parser state must be >= kParsingClusters for output to be valid.
  Status OnlyOneStream(bool* value) const;

  // Parses the next WebM chunk in |buf|. If a level 1 WebM element is parsed
  // the size of the element is passed back in |ptr_element_size|. Data in |buf|
  // must not be removed unitl the offset is passed back in |ptr_element_size|.
  // Returns the current state of the parser or returns kParsingError if the
  // parser encountered an error.
  Status ParseNextChunk(const Buffer& buf, int32* ptr_element_size);

  // Sets the reader end of file offset.
  bool SetEndOfFilePosition(int64 offset);

  // Returns average bits per second for the first track of track |type|.
  // Parser state must equal kDoneParsing for output to be valid.
  Status TrackAverageBitsPerSecond(TrackTypes type, int64* average_bps) const;

  // Returns number of tracks for track of |type|. Parser state must be >=
  // kParsingClusters for output to be valid.
  Status TrackCount(TrackTypes type, int64* count) const;

  // Returns number of frames for the first track of |type|. Parser state must
  // equal kDoneParsing for output to be valid.
  Status TrackFrameCount(TrackTypes type, int64* count) const;

  // Returns size in bytes for the first track of |type|. Parser state must
  // equal kDoneParsing for output to be valid.
  Status TrackSize(TrackTypes type, int64* size) const;

  // Returns start time in nanoseconds for the first track of |type|. Parser
  // state must be >= kParsingClusters for output to be valid.
  Status TrackStartNanoseconds(TrackTypes type, int64* start) const;

  // Returns true if the file contains at least one video track. Parser state
  // must be >= kParsingClusters for output to be valid.
  Status HasVideo(bool* value) const;

  // Returns the height in pixels of the first video track. Returns 0 if there
  // is no video track. Parser state must be >= kParsingClusters for output to
  // be valid.
  Status VideoHeight(int* height) const;

  // Returns the width in pixels of the first video track. Returns 0 if there
  // is no video track. Parser state must be >= kParsingClusters for output to
  // be valid.
  Status VideoWidth(int* width) const;

 private:
  // Parse function pointer type.
  typedef Status (WebMIncrementalParser::*ParseFunc)(int32* ptr_element_size);

  // Calculate and returns average bits per second for the WebM file starting
  // from |cp|. If |cp| is NULL calculate the bits per second over the entire
  // file. Returns 0 on error.
  int64 CalculateBitsPerSecond(const mkvparser::CuePoint* cp) const;

  // Returns the frame rate for |track_number|. The frame rate is calculated
  // from all the frames in the Clusters. Returns 0.0 if it cannot calculate
  // the frame rate.
  double CalculateFrameRate(int track_number) const;

  // Returns average bits per second for |track_number| starting from |cp|. If
  // |cp| is NULL calculate the bits per second over the entire file.
  // Returns 0 on error.
  Status CalculateTrackBitsPerSecond(int track_number,
                                     const mkvparser::CuePoint* cp,
                                     int64* average_bps) const;

  // Returns the number of frames for |track_number| starting from |cp|. If
  // |cp| is NULL calculate the number of frames over the entire file. Returns
  // 0 on error.
  int64 CalculateTrackFrameCount(int track_number,
                                 const mkvparser::CuePoint* cp) const;

  // Returns size in bytes for |track_number| starting from |cp|. If |cp| is
  // NULL calculate the size over the entire file. Returns 0 on error.
  int64 CalculateTrackSize(int track_number,
                           const mkvparser::CuePoint* cp) const;

  // Returns true if the first Block of every CuePoint is the first Block in
  // the Cluster for that track.
  bool CuesFirstInCluster(TrackTypes type) const;

  // Calculates private per Track statistics on the WebM file. This function
  // parses all of the Blocks within the file and stores per Track information
  // to query later. This is an optimization as parsing every Block in a WebM
  // file can take a long time. Returns true on success.
  bool GenerateStats();

  // Return the first audio track. Returns NULL if there are no audio tracks.
  const mkvparser::AudioTrack* GetAudioTrack() const;

  // Returns the Cues from the webm file.
  const mkvparser::Cues* GetCues() const;

  // Returns true if the Block of |track| within the Cluster represented by
  // index is valid. |cp| is the CuePoint that references the Cluster to
  // search. |track| the Track to look for. |index| is the index of the Block.
  // |block| is the output variable to get the Block. Returns false if the
  // function cannot find the Block referenced by |track| and |index|. Returns
  // false if |cluster| or |block| is NULL.
  bool GetIndexedBlock(const mkvparser::CuePoint& cp,
                       const mkvparser::Track& track,
                       int index,
                       const mkvparser::Cluster** cluster,
                       const mkvparser::Block** block) const;

  // Returns the Track by an index. Returns NULL if it cannot find the track
  // represented by |index|.
  const mkvparser::Track* GetTrack(uint32 index) const;

  // Return the first video track. Returns NULL if there are no video tracks.
  const mkvparser::VideoTrack* GetVideoTrack() const;

  // Tries to parse a cluster.  Returns |kNeedMoreData| when more data is
  // needed. Returns |kSuccess| and sets |ptr_element_size| when all cluster
  // data has been parsed.
  Status ParseCluster(int32* ptr_element_size);

  // Tries to parse the segment headers: segment info and segment tracks.
  // Returns |kNeedMoreData| if more data is needed.  Returns |kSuccess| and
  // sets |ptr_element_size| when successful.
  Status ParseSegmentHeaders(int32* ptr_element_size);

  // Returns true if |block| is a key frame. |cp| is the CuePoint that
  // references the Cluster to search. |cluster| is the Cluster that contains
  // |block|. |block| is the Block to check.
  bool StartsWithKey(const mkvparser::CuePoint& cp,
                     const mkvparser::Cluster& cluster,
                     const mkvparser::Block& block) const;

  // Flag telling if the internal per Track statistics have been calculated.
  bool calculated_file_stats_;

  // Bytes read in partially parsed cluster.
  int64 cluster_parse_offset_;

  // Parsing function-- either |ParseSegmentHeaders| or |ParseCluster|.
  ParseFunc parse_func_;

  // Pointer to current cluster when |ParseCluster| only partially parses
  // cluster data.  NULL otherwise. Note that |ptr_cluster_| is memory owned by
  // libwebm's mkvparser.
  const mkvparser::Cluster* ptr_cluster_;

  // Buffer object that implements the IMkvReader interface required by
  // libwebm's mkvparser using a window into the |buf| argument passed to
  // |Parse|.
  std::auto_ptr<WebmIncrementalReader> reader_;

  // Pointer to libwebm segment; needed for cluster parsing operations.
  std::auto_ptr<mkvparser::Segment> segment_;

  // The current state of the parser. The state starts in kParsingHeaders.
  // After the parser has parsed all of the header data the state will
  // transition to kParsingClusters. After the parser has finished parsing all
  // of the cluster the state will transisiotn to kDoneParsing.
  Status state_;

  // Sum of parsed element lengths.  Used to update |parser_| window.
  int64 total_bytes_parsed_;

  // Member variables used to calculate information about the WebM file which
  // only need to be parsed once. Key is the Track number.
  // |tracks_size_| Size in bytes of all Blocks per Track.
  std::map<int, int64> tracks_size_;

  // Count of all Blocks per Track.
  std::map<int, int64> tracks_frame_count_;

  // Start time in milliseconds per Track.
  std::map<int, int64> tracks_start_milli_;

  WEBM_TOOLS_DISALLOW_COPY_AND_ASSIGN(WebMIncrementalParser);
};

}  // namespace webm_tools

#endif  // SHARED_WEBM_INCREMENTAL_PARSER_H_
