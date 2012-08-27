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
class Cues;
class CuePoint;
class MkvReader;
class Segment;
class SegmentInfo;
class Track;
class VideoTrack;
}  // namespace mkvparser

namespace webm_tools {

struct CueDesc {
  int64 start_time_ns;
  int64 end_time_ns;
  int64 start_offset;
  int64 end_offset;
};

// This class is used to load a WebM file using libwebm. The class adds
// convenience functions to gather information about WebM files. The class is
// dependent on libwebm. Code using this class must call Init() before other
// functions may be called.
class WebMFile {
 public:
  enum TrackTypes {
    kUnknown = 0,
    kVideo = 1,
    kAudio = 2,
  };
  explicit WebMFile(const std::string& filename);
  ~WebMFile();

  // Load and parse the webm file. Returns false if the DocType is not "webm"
  // Returns true if the file has been loaded and verified.
  bool Init();

  // Returns true if the file contains at least one audio track.
  bool HasAudio() const;

  // Returns the number of channels of the first audio track. Returns 0 if
  // there is no audio track.
  int AudioChannels() const;

  // Returns the sample rate of the first audio track. Returns 0 if there is
  // no audio track.
  int AudioSampleRate() const;

  // Returns the audio sample size in bits per sample of the first audio track.
  // Returns 0 if there no audio track.
  int AudioSampleSize() const;

  // Returns how many seconds are in |buffer| after |search_sec| has passed.
  // |time| is the start time in seconds. |search_sec| is the number of
  // seconds to emulate downloading data. |kbps| is the current download
  // datarate. |buffer| is an input/output parameter. The amount of time in
  // seconds will be added to the value passed into |buffer|. |sec_counted|
  // is the time in seconds used to perform the calculation. |sec_counted|
  // may be different than |search_sec| is it is near the end of the clip.
  // Return values < 0 are errors. Return value of 0 is success.
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
  // Return value of 1 is the function encountered a buffer underrun.
  int BufferSizeAfterTimeDownloaded(int64 time_ns,
                                    double search_sec,
                                    int64 bps,
                                    double min_buffer,
                                    double* buffer,
                                    double* sec_to_download) const;

  // Returns the average framerate of the first video track. Returns 0.0 if
  // there is no video track or we cannot calculate an average framerate.
  double CalculateVideoFrameRate() const;

  // Returns true if the TrackNumber, CodecID and CodecPrivate in the webm
  // file are equal to the values in |webm_file|.
  bool CheckBitstreamSwitching(const WebMFile& webm_file) const;

  // Returns true if the start time and the block number of all the cue
  // points in the webm file are equal to all of the cue points in
  // |webm_file|.
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
  // |output_alignment_times|. |output_string| may be NULL.
  static bool CheckCuesAlignmentList(
      const std::vector<const WebMFile*>& webm_list,
      double seconds,
      bool check_for_sap,
      bool check_for_audio_match,
      bool verbose,
      bool output_alignment_times,
      bool output_alignment_stats,
      std::string* output_string);

  // Returns true if the file has a Cues element.
  bool CheckForCues() const;

  // Returns true if the first Block of every CuePoint is the first Block in
  // the Cluster for that track.
  bool CuesFirstInCluster(TrackTypes type) const;

  // Calculate and returns average bits per second for the WebM file.
  int64 FileAverageBitsPerSecond() const;

  // Returns the length of the file in bytes.
  int64 FileLength() const;

  // Calculate and return maximum bits per second for the WebM file.
  int64 FileMaximumBitsPerSecond() const;

  // Returns the codec string associated with the file. If the CodecID
  // is V_VP8 then the string returned will be "vp8". If the CodecID is
  // A_VORBIS then the string returned will be "vorbis". If there is more
  // than one track in the file the codecs will be in a comma separated list
  // like "vp8, vorbis". If the CodecID is anything else then the string
  // returned will be empty.
  std::string GetCodec() const;

  // Returns the Cues from the webm file.
  const mkvparser::Cues* GetCues() const;

  // Returns the duration of the file in nanoseconds.
  int64 GetDurationNanoseconds() const;

  // Returns the byte offset in the file for the start of the Segment Info and
  // Tracks element starting with the EBML element ID to the end offset of the
  // element. A return value of -1 for either value indicates an error.
  void GetHeaderRange(int64* start, int64* end) const;

  // Returns the mimetype string associated with the file. Returns
  // "video/webm" if the file is a valid WebM file. Returns the empty string
  // if not a valid WebM file.
  std::string GetMimeType() const;

  // Returns the mimetype with the codec parameter for the first two tracks
  // in the file. The format is defined by the WebM specification. Returns the
  // empty string if not a valid WebM file.
  std::string GetMimeTypeWithCodec() const;

  // Returns the SegmentInfo element.
  const mkvparser::SegmentInfo* GetSegmentInfo() const;

  // Returns the starting byte offset the segment element.
  int64 GetSegmentStartOffset() const;

  // Returns true if the first video track equals V_VP8 or the first audio
  // track equals A_VORBIS. Returns false if there are no audio or video
  // tracks. Returns false if there is both a video tack and an audio track.
  // This is because for adaptive streaming we want the streams to be separate.
  bool OnlyOneStream() const;

  // Returns the peak bits per second over the entire file taking into account a
  // prebuffer of |prebuffer_ns|. This function will iterate over all the Cue
  // points to get the maximum bits per second from all Cue points. Return
  // values < 0 are errors.
  int64 PeakBitsPerSecondOverFile(int64 prebuffer_ns) const;

  // Returns average bits per second for the first track of track |type|.
  // Returns 0 on error.
  int64 TrackAverageBitsPerSecond(TrackTypes type) const;

  // Returns number of tracks for track of |type|.
  int64 TrackCount(TrackTypes type) const;

  // Returns number of frames for the first track of |type|. Returns 0 on error.
  int64 TrackFrameCount(TrackTypes type) const;

  // Returns size in bytes for the first track of |type|. Returns 0 on error.
  int64 TrackSize(TrackTypes type) const;

  // Returns start time in nanoseconds for the first track of |type|. Returns 0
  // on error.
  int64 TrackStartNanoseconds(TrackTypes type) const;

  // Returns true if the file contains at least one video track.
  bool HasVideo() const;

  // Returns the average framerate of the first video track. Returns 0.0 if
  // there is no video track or there is no FrameRate element.
  double VideoFramerate() const;

  // Returns the height in pixels of the first video track. Returns 0 if there
  // is no video track.
  int VideoHeight() const;

  // Returns the width in pixels of the first video track. Returns 0 if there
  // is no video track.
  int VideoWidth() const;

  const std::string& filename() const { return filename_; }

 private:
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

  // Time in nano seconds to split up the Cues element into the chunkindexlist.
  int64 cue_chunk_time_nano_;

  // CueDesc list.
  std::vector<CueDesc> cue_desc_list_;

  // Path to WebM file.
  const std::string filename_;

  // libwebm reader
  std::auto_ptr<mkvparser::MkvReader> reader_;

  // Main WebM element.
  std::auto_ptr<mkvparser::Segment> segment_;

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
