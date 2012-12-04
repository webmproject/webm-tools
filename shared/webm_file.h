/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SHARED_WEBM_FILE_H_
#define SHARED_WEBM_FILE_H_

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
class IMkvReader;
class MkvReader;
class Segment;
class SegmentInfo;
class Track;
class VideoTrack;
}  // namespace mkvparser

namespace webm_tools {

class WebmIncrementalReader;

struct CueDesc {
  int64 start_time_ns;
  int64 end_time_ns;
  int64 start_offset;
  int64 end_offset;
};

// This class is used to load a WebM file using libwebm. The class adds
// convenience functions to gather information about WebM files. The class is
// dependent on libwebm.
//
// WebMFile can be used to parse a WebM file incrementally or to parse the
// entire file. To parse the entire file call ParseFile() after the class is
// created. To parse a WebM file incrementally call ParseNextChunk with
// sequential chunks. If a level 1 WebM element is parsed the size of the
// element is passed back. The calling code is responsible for removing the
// data of the element that was just parsed. Once the end of the file is known
// a call to SetEndOfFilePosition should be made.
//
// The code using WebMFile must pay attention to the state of WebMFile as the
// values returned may change over time if parsing incrementally. The state
// starts in kParsingHeaders. After the parser has parsed all of the header
// data the state will transition to kParsingClusters. After the parser has
// finished parsing all of the cluster the state will transition to
// kParsingDone. Check the function comments as to what state the parser needs
// to be in to output valid values.
class WebMFile {
 public:
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
    kParsingDone = 4,
  };

  WebMFile();
  ~WebMFile();

  // Loads and parses the webm file. Returns false if the DocType is not "webm"
  // Returns true if the file has been loaded and verified.
  bool ParseFile(const std::string& filename);

  // Loads and parses the webm file. |reader| is an object that implements the
  // mkvparser::IMkvReader interface. Returns false if the DocType is not
  // "webm". Returns true if the file has been loaded and verified.
  bool ParseFile(mkvparser::IMkvReader* reader);

  // Returns true if the file contains at least one audio track. Parser
  // state must be >= kParsingClusters for output to be valid.
  bool HasAudio() const;

  // Returns the number of channels of the first audio track. Returns 0 if
  // there is no audio track. Parser state must be >= kParsingClusters for
  // output to be valid.
  int AudioChannels() const;

  // Returns the sample rate of the first audio track. Returns 0 if there is
  // no audio track. Parser state must be >= kParsingClusters for output to be
  // valid.
  int AudioSampleRate() const;

  // Returns the audio sample size in bits per sample of the first audio track.
  // Returns 0 if there no audio track. Parser state must be >=
  // kParsingClusters for output to be valid.
  int AudioSampleSize() const;

  // Returns how many seconds are in |buffer| after |search_sec| has passed.
  // |time| is the start time in seconds. |search_sec| is the number of
  // seconds to emulate downloading data. |kbps| is the current download
  // datarate. |buffer| is an input/output parameter. The amount of time in
  // seconds will be added to the value passed into |buffer|. |sec_counted|
  // is the time in seconds used to perform the calculation. |sec_counted|
  // may be different than |search_sec| is it is near the end of the clip.
  // Return values < 0 are errors. Return value of 0 is success. Parser state
  // must equal kParsingDone for output to be valid.
  int BufferSizeAfterTime(double time,
                          double search_sec,
                          int64 kbps,
                          double* buffer,
                          double* sec_counted) const;

  // Returns how many seconds are in |buffer| and how many seconds it took to
  // download |search_sec| seconds. |time_ns| is the start time in nanoseconds.
  // |search_sec| is the time in seconds to emulate downloading. |bps| is
  // the current download datarate in bits per second. |min_buffer| is the
  // amount of time in seconds that buffer must stay above or return a buffer
  // underrun condition. |buffer| is an input/output parameter. The amount of
  // time in seconds will be added to the value passed into |buffer|.
  // |sec_to_download| is the time in seconds that it took to download the
  // data. Return values < 0 are errors. Return value of 0 is success.
  // Return value of 1 is the function encountered a buffer underrun. Parser
  // state must equal kParsingDone for output to be valid.
  int BufferSizeAfterTimeDownloaded(int64 time_ns,
                                    double search_sec,
                                    int64 bps,
                                    double min_buffer,
                                    double* buffer,
                                    double* sec_to_download) const;

  // Returns the average framerate of the first video track. Returns 0.0 if
  // there is no video track or we cannot calculate an average framerate.
  // Parser state must equal kParsingDone for output to be valid.
  double CalculateVideoFrameRate() const;

  // Returns true if the TrackNumber, CodecID and CodecPrivate in the webm
  // file are equal to the values in |webm_file|. Parser states must be >=
  // kParsingClusters for output to be valid.
  bool CheckBitstreamSwitching(const WebMFile& webm_file) const;

  // Returns true if the start time and the block number of all the cue
  // points in the webm file are equal to all of the cue points in
  // |webm_file|. Parser states must equal kParsingDone for output to be valid.
  bool CheckCuesAlignment(const WebMFile& webm_file) const;

  // Returns true if the CuePoints across |webm_list| are aligned with respect
  // to time. |seconds| is the range in seconds that the function is allowed to
  // search for alignment. I.e. If file A had a CuePoint every 5 seconds and
  // file B had a CuePoint every 15 seconds then the files would be aligned
  // if |seconds| >= 15.0. |check_for_sap| if true checks if potentially aligned
  // CuePoints start with a key frame. I.e. The first frame in the Custer is a
  // key frame. |check_for_audio_match| if true checks that the first audio
  // block in potentially aligned CuePoints are the same. |verbose| if true
  // outputs more information to stdout. |output_alignment_times| if true will
  // append the aligned CuePoint times in seconds to |output_string|.
  // |output_alignment_stats| if true will append the aligned CuePoint times in
  // seconds and the reason why potentially aligned CuePoints were rejected.
  // |output_alignment_stats| supersedes |output_alignment_times|.
  // |output_string| is an output parameter with information on why the function
  // returned false and/or the output from |output_alignment_stats| or
  // |output_alignment_times|. |output_string| may be NULL. Parser states must
  // equal kParsingDone for output to be valid.
  static bool CheckCuesAlignmentList(
      const std::vector<const WebMFile*>& webm_list,
      double seconds,
      bool check_for_sap,
      bool check_for_audio_match,
      bool verbose,
      bool output_alignment_times,
      bool output_alignment_stats,
      std::string* output_string);

  // Returns true if the file has a Cues element. Parser state must equal
  // kParsingDone for output to be valid.
  bool CheckForCues() const;

  // Returns true if the first Block of every CuePoint is the first Block in
  // the Cluster for that track. Parser state must equal kParsingDone for
  // output to be valid.
  bool CuesFirstInCluster(TrackTypes type) const;

  // Calculate and returns average bits per second for the WebM file. Parser
  // state must be >= kParsingClusters and the end position must be set for
  // output to be valid.
  int64 FileAverageBitsPerSecond() const;

  // Returns the length of the file in bytes. Parser state must be >=
  // kParsingClusters and the end position must be set for output to be valid.
  int64 FileLength() const;

  // Calculates and returns maximum bits per second for the WebM file. Parser
  // state must equal kParsingDone for output to be valid.
  int64 FileMaximumBitsPerSecond() const;

  // Returns the codec string associated with the file. If the CodecID
  // is V_VP8 then the string returned will be "vp8". If the CodecID is
  // A_VORBIS then the string returned will be "vorbis". If there is more
  // than one track in the file the codecs will be in a comma separated list
  // like "vp8, vorbis". If the CodecID is anything else then the string
  // returned will be empty. Parser state must be >= kParsingClusters for
  // output to be valid.
  std::string GetCodec() const;

  // Returns the Cues from the webm file. Parser state must equal kParsingDone
  // for output to be valid.
  const mkvparser::Cues* GetCues() const;

  // Returns the duration of the file in nanoseconds. Parser state must be >=
  // kParsingClusters for output to be valid.
  int64 GetDurationNanoseconds() const;

  // Returns the byte offset in the file for the start of the Segment Info and
  // Tracks element starting with the EBML element ID to the end offset of the
  // element. A return value of -1 for either value indicates an error.
  void GetHeaderRange(int64* start, int64* end) const;

  // Returns the mimetype string associated with the file. Returns
  // "video/webm" if the file is a valid WebM file. Returns the empty string
  // if not a valid WebM file. Parser state must be >= kParsingClusters for
  // output to be valid.
  std::string GetMimeType() const;

  // Returns the mimetype with the codec parameter for the first two tracks
  // in the file. The format is defined by the WebM specification. Returns the
  // empty string if not a valid WebM file. Parser state must be >=
  // kParsingClusters for output to be valid.
  std::string GetMimeTypeWithCodec() const;

  // Returns the Segment element. Returns NULL if the segment has not been
  // created.
  const mkvparser::Segment* GetSegment() const;

  // Returns the SegmentInfo element. Parser state must be >= kParsingClusters
  // for output to be valid.
  const mkvparser::SegmentInfo* GetSegmentInfo() const;

  // Returns the starting byte offset for the Segment element. Parser state must
  // be >= kParsingClusters for output to be valid.
  int64 GetSegmentStartOffset() const;

  // Returns true if the first video track equals V_VP8 or the first audio
  // track equals A_VORBIS. Returns false if there are no audio or video
  // tracks. Returns false if there is both a video tack and an audio track.
  // Parser state must be >= kParsingClusters for output to be valid.
  bool OnlyOneStream() const;

  // Parses the next WebM chunk in |data|. The application owns |data|. If one
  // or more level 1 WebM elements are parsed the amount of bytes read is
  // passed back in |bytes_read| and the application must adjust |data| by
  // |bytes_read| on the next call to ParseNextChunk. If |bytes_read| is -1 the
  // application should append more to |data|, but where |data| points to must
  // not be changed on subsequent calls to ParseNextChunk. Returns the current
  // state of the parser or returns kParsingError if the parser encountered an
  // error.
  Status ParseNextChunk(const uint8* data, int32 size, int32* bytes_read);

  // Returns the peak bits per second over the entire file taking into account a
  // prebuffer of |prebuffer_ns|. This function will iterate over all the Cue
  // points to get the maximum bits per second from all Cue points. Return
  // values < 0 are errors. Parser state must equal kParsingDone for output to
  // be valid.
  int64 PeakBitsPerSecondOverFile(int64 prebuffer_ns) const;

  // Sets the reader end of file offset.
  bool SetEndOfFilePosition(int64 offset);

  // Returns average bits per second for the first track of track |type|.
  // Parser state must equal kParsingDone for output to be valid.
  int64 TrackAverageBitsPerSecond(TrackTypes type) const;

  // Returns number of tracks for track of |type|. Parser state must be >=
  // kParsingClusters for output to be valid.
  int64 TrackCount(TrackTypes type) const;

  // Returns number of frames for the first track of |type|. Parser state must
  // equal kParsingDone for output to be valid.
  int64 TrackFrameCount(TrackTypes type) const;

  // Returns size in bytes for the first track of |type|. Parser state must
  // equal kParsingDone for output to be valid.
  int64 TrackSize(TrackTypes type) const;

  // Returns start time in nanoseconds for the first track of |type|. Parser
  // state must be >= kParsingClusters for output to be valid.
  int64 TrackStartNanoseconds(TrackTypes type) const;

  // Returns true if the file contains at least one video track. Parser state
  // must be >= kParsingClusters for output to be valid.
  bool HasVideo() const;

  // Returns the average framerate of the first video track. Returns 0.0 if
  // there is no video track or there is no FrameRate element.
  double VideoFramerate() const;

  // Returns the height in pixels of the first video track. Returns 0 if there
  // is no video track. Parser state must be >= kParsingClusters for output to
  // be valid.
  int VideoHeight() const;

  // Returns the width in pixels of the first video track. Returns 0 if there
  // is no video track. Parser state must be >= kParsingClusters for output to
  // be valid.
  int VideoWidth() const;

  const std::string& filename() const { return filename_; }
  Status state() const { return state_; }
  mkvparser::IMkvReader* reader() { return reader_; }

 private:
  // Parse function pointer type.
  typedef Status (WebMFile::*ParseFunc)(int32* bytes_read);

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
  int64 CalculateTrackBitsPerSecond(int track_number,
                                    const mkvparser::CuePoint* cp) const;

  // Returns the number of frames for |track_number| starting from |cp|. If
  // |cp| is NULL calculate the number of frames over the entire file. Returns
  // 0 on error.
  int64 CalculateTrackFrameCount(int track_number,
                                 const mkvparser::CuePoint* cp) const;

  // Returns size in bytes for |track_number| starting from |cp|. If |cp| is
  // NULL calculate the size over the entire file. Returns 0 on error.
  int64 CalculateTrackSize(int track_number,
                           const mkvparser::CuePoint* cp) const;

  // Returns true if the first four bytes of |doc_type| match "webm".
  bool CheckDocType(const std::string& doc_type) const;

  // Returns the |start| and |end| byte offsets and the start and end times
  // of the requested chunk. |start_time_nano| is the time in nano seconds
  // inclusive to start searching for in the Cues element. |end_time_nano| is
  // the time in nano seconds exclusive to end searching for in the Cues
  // element. If you want to get the entries Cues element set |end_time_nano|
  // to max of webm_tools::int64.
  void FindCuesChunk(int64 start_time_nano,
                     int64 end_time_nano,
                     int64* start,
                     int64* end,
                     int64* cue_start_time,
                     int64* cue_end_time) const;

  // Calculates private per Track statistics on the WebM file. This function
  // parses all of the Blocks within the file and stores per Track information
  // to query later. This is an optimization as parsing every Block in a WebM
  // file can take a long time. Returns true on success.
  bool GenerateStats();

  // Return the first audio track. Returns NULL if there are no audio tracks.
  const mkvparser::AudioTrack* GetAudioTrack() const;

  // Returns the byte offset in the file for the start of the first Cluster
  // element starting with the EBML element ID. A value of -1 indicates there
  // was an error.
  int64 GetClusterRangeStart() const;

  // Returns the CueDesc associated with |time|. |time| is the time in
  // nanoseconds. Returns NULL if it cannot find a CueDesc.
  const CueDesc* GetCueDescFromTime(int64 time) const;

  // Returns the time in nanoseconds of the first Block in the Cluster
  // referenced by |cp|. |cp| is the CuePoint that references the Cluster to
  // search. |track_num| is the number of the Track to look for the first
  // Block. |nanoseconds| is the output time in nanoseconds. |nanoseconds| is
  // set only if the function returns true. Returns false if it could not
  // find a Block of |track_num| within the Cluster. Returns false if
  // nanoseconds is NULL.
  bool GetFirstBlockTime(const mkvparser::CuePoint& cp,
                         int track_num,
                         int64* nanoseconds) const;

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

  // Returns the byte offset in the file for the start of the SegmentInfo
  // element starting with the EBML element ID to the end offset of the
  // element.
  void GetSegmentInfoRange(int64* start, int64* end) const;

  // Returns the Track by an index. Returns NULL if it cannot find the track
  // represented by |index|.
  const mkvparser::Track* GetTrack(uint32 index) const;

  // Returns the byte offset in the file for the start of the Tracks element
  // starting with the EBML element ID to the end offset of the element.
  void GetTracksRange(int64* start, int64* end) const;

  // Return the first video track. Returns NULL if there are no video tracks.
  const mkvparser::VideoTrack* GetVideoTrack() const;

  // Tries to parse a cluster.  Returns |kParsingClusters| and sets
  // |bytes_read| to -1 if more data is needed. Returns |kParsingClusters| and
  // sets |bytes_read| to the amount of bytes read when a Cluster has been
  // parsed. Returns |kParsingError| on error.
  Status ParseCluster(int32* bytes_read);

  // Tries to parse all of the elements until the first Cluster. Returns
  // |kParsingHeader| and sets |bytes_read| to -1 if more data is needed.
  // Returns |kParsingClusters| and sets |bytes_read| to the amount of bytes
  // read when successful. Returns |kParsingError| on error.
  Status ParseSegmentHeaders(int32* bytes_read);

  // Populates |cue_desc_list_| from the Cues element. Returns true on success.
  bool LoadCueDescList();

  // Returns true if |block| is an altref frame.
  bool IsFrameAltref(const mkvparser::Block& block) const;

  // Returns the peak bits per second starting at |time_ns| taking into
  // account a prebuffer of |prebuffer_ns|. The peak bits per second is
  // returned in the out parameter |bits_per_second|. Return values < 0 are
  // errors. Return value of 0 is success.
  int PeakBitsPerSecond(int64 time_ns,
                        int64 prebuffer_ns,
                        double* bits_per_second) const;

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

  // Time in nano seconds to split up the Cues element into the chunkindexlist.
  int64 cue_chunk_time_nano_;

  // CueDesc list.
  std::vector<CueDesc> cue_desc_list_;

  // Variable to hold end of file position until reader is created. -1
  // indicates the file position has not been set.
  int64 end_of_file_position_;

  // Calculated file duration in nanoseconds.
  int64 file_duration_nano_;

  // Path to WebM file.
  std::string filename_;

  // Parsing function-- either |ParseSegmentHeaders| or |ParseCluster|.
  ParseFunc parse_func_;

  // Pointer to current cluster when |ParseCluster| only partially parses
  // cluster data.  NULL otherwise. Note that |ptr_cluster_| is memory owned by
  // libwebm's mkvparser.
  const mkvparser::Cluster* ptr_cluster_;

  // Base IMkvReader interface that gets set to |file_reader_| if ParseFile is
  // called or |incremental_reader_| if parsing a WebM file incrementally.
  mkvparser::IMkvReader* reader_;

  // libwebm file reader that implements the IMkvReader interface required by
  // libwebm's mkvparser.
  std::auto_ptr<mkvparser::MkvReader> file_reader_;

  // Buffer object that implements the IMkvReader interface required by
  // libwebm's mkvparser using a window into the |buf| argument passed to
  // |Parse|.
  std::auto_ptr<WebmIncrementalReader> incremental_reader_;

  // Pointer to libwebm segment.
  std::auto_ptr<mkvparser::Segment> segment_;

  // The current state of the parser. The state starts in kParsingHeaders.
  // After the parser has parsed all of the header data the state will
  // transition to kParsingClusters. After the parser has finished parsing all
  // of the cluster the state will transition to kParsingDone.
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

  WEBM_TOOLS_DISALLOW_COPY_AND_ASSIGN(WebMFile);
};

}  // namespace webm_tools

#endif  // SHARED_WEBM_FILE_H_
