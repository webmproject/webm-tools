/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBM_DASH_MANIFEST_WEBM_FILE_H_
#define WEBM_DASH_MANIFEST_WEBM_FILE_H_

#include <memory>
#include <string>
#include <vector>

namespace indent_webm {
class Indent;
}  // namespace indent_webm

namespace mkvparser {
class AudioTrack;
class Cues;
class CuePoint;
struct EBMLHeader;
class MkvReader;
class Segment;
class Track;
class VideoTrack;
}  // namespace mkvparser

namespace webm_dash {

struct CueDesc {
  long long start_time_ns;
  long long end_time_ns;
  long long start_offset;
  long long end_offset;
};

// This class is used to load a WebM file using libwebm. The class adds
// convenience functions to gather information about WebM files. The class is
// dependent on libwebm. Code using this class must call Init() before other
// functions may be called.
class WebMFile {
 public:
  explicit WebMFile(const std::string& filename);
  ~WebMFile();

  // Load and parse the webm file. Returns false if the DocType is not "webm"
  // Returns true if the file has been loaded and verified.
  bool Init();

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
                          long long kbps,
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
  // data.
  // Return values < 0 are errors. Return value of 0 is success.
  // Return value of 1 is the function encountered a buffer underrun.
  int BufferSizeAfterTimeDownloaded(long long time_ns,
                                    double search_sec,
                                    long long bps,
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
  bool CheckCuesAlignement(const WebMFile& webm_file) const;

  // Returns true if the file has a Cues element.
  bool CheckForCues() const;

  // Returns true if the first Block of every CuePoint is the first Block in
  // the Cluster for that track.
  bool CuesFirstInCluster() const;

  // Returns the number of channels in the first audio track. Returns 0 if
  // there is no audio track.
  int GetAudioChannels() const;

  // Returns the sample rate in the first audio track. Returns 0 if there is
  // no audio track.
  int GetAudioSampleRate() const;

  // Calculate and return average bandwidth for the WebM file in kilobits
  // per second.
  long long GetAverageBandwidth() const;

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
  long long GetDurationNanoseconds() const;

  // Returns the byte offset in the file for the start of the Segment Info and
  // Tracks element starting with the EBML element ID to the end offset of the
  // element. A return value of -1 for either value indicates an error.
  void GetHeaderRange(long long* start, long long* end) const;

  // Calculate and return maximum bandwidth for the WebM file in kilobits
  // per second.
  long long GetMaximumBandwidth() const;

  // Returns the mimetype string associated with the file. Returns
  // "video/webm" if the file is a valid WebM file. Returns the empty string
  // if not a valid WebM file.
  std::string GetMimeType() const;

  // Returns the mimetype with the codec parameter for the first two tracks
  // in the file. The format is defined by the WebM specification. Returns the
  // empty string if not a valid WebM file.
  std::string GetMimeTypeWithCodec() const;

  // Returns the starting byte offset the segment element.
  long long GetSegmentStartOffset() const;

  // Returns the average framerate of the first video track. Returns 0.0 if
  // there is no video track or there is no FrameRate element.
  double GetVideoFramerate() const;

  // Returns the height in pixels of the first video track. Returns 0 if there
  // is no video track.
  int GetVideoHeight() const;

  // Returns the width in pixels of the first video track. Returns 0 if there
  // is no video track.
  int GetVideoWidth() const;

  // Returns true if the first video track equals V_VP8 or the first audio
  // track equals A_VORBIS. Returns false if there are no audio or video
  // tracks. Returns false if there is both a video tack and an audio track.
  // This is because for adaptive streaming we want the streams to be separate.
  bool OnlyOneStream() const;

  // Returns the peak bandwidth over the entire file taking into account a
  // prebuffer of |prebuffer_ns|. This function will iterate over all the Cue
  // points to get the maximum bandwidth from all Cue points.Return values < 0
  // are errors.
  long long PeakBandwidthOverFile(long long prebuffer_ns) const;

  const std::string& filename() const { return filename_; }

 private:
  // Calculate and return average bandwidth for the WebM file in kilobits
  // per second starting from |cp|. If |cp| is NULL calculate the bandwidth
  // over the entire file. Return 0 on error.
  long long CalculateBandwidth(const mkvparser::CuePoint* cp) const;

  // Returns the frame rate for |track_number|. The frame rate is calculated
  // from all the frames in the Clusters. Returns 0.0 if it cannot calculate
  // the frame rate.
  double CalculateFrameRate(long long track_number) const;

  // Returns true if the first four bytes of the doctype of the WebM file
  // match "webm".
  bool CheckDocType() const;

  // Returns the |start| and |end| byte offsets and the start and end times
  // of the requested chunk. |start_time_nano| is the time in nano seconds
  // inclusive to start searching for in the Cues element. |end_time_nano| is
  // the time in nano seconds exclusive to end searching for in the Cues
  // element. If you want to get the entries Cues element set |end_time_nano|
  // to max of long long.
  void FindCuesChunk(long long start_time_nano,
                     long long end_time_nano,
                     long long* start,
                     long long* end,
                     long long* cue_start_time,
                     long long* cue_end_time) const;

  // Return the first audio track. Returns NULL if there are no audio tracks.
  const mkvparser::AudioTrack* GetAudioTrack() const;

  // Returns the byte offset in the file for the start of the first Cluster
  // element starting with the EBML element ID. A value of -1 indicates there
  // was an error.
  long long GetClusterRangeStart() const;

  // Returns the CueDesc associated with |time|. |time| is the time in
  // nanoseconds. Returns NULL if it cannot find a CueDesc.
  const CueDesc* GetCueDescFromTime(long long time) const;

  // Returns the byte offset in the file for the start of the SegmentInfo
  // element starting with the EBML element ID to the end offset of the
  // element.
  void GetSegmentInfoRange(long long* start, long long* end) const;

  // Returns the Track by an index. Returns NULL if it cannot find the track
  // represented by |index|.
  const mkvparser::Track* GetTrack(unsigned int index) const;

  // Returns the byte offset in the file for the start of the Tracks element
  // starting with the EBML element ID to the end offset of the element.
  void GetTracksRange(long long* start, long long* end) const;

  // Return the first video track. Returns NULL if there are no video tracks.
  const mkvparser::VideoTrack* GetVideoTrack() const;

  // Populates the CueDesc list form the Cues element. Returns true on success.
  bool LoadCueDescList();

  // Returns the peak bandwidth starting at |time_ns| taking into account a
  // prebuffer of |prebuffer_ns|. The peak bandwidth is returned in the out
  // parameter |bandwidth|. Return values < 0 are errors. Return value of 0
  // is success.
  int PeakBandwidth(long long time_ns,
                    long long prebuffer_ns,
                    double* bandwidth) const;

  // Time in nano seconds to split up the Cues element into the chunkindexlist.
  long long cue_chunk_time_nano_;

  // CuesDesc list.
  std::vector<CueDesc> cue_desc_list_;

  // Path to WebM file.
  const std::string filename_;

  // EBML header of the file.
  std::auto_ptr<mkvparser::EBMLHeader> ebml_header_;

  // libwebm reader
  std::auto_ptr<mkvparser::MkvReader> reader_;

  // Main WebM element.
  std::auto_ptr<mkvparser::Segment> segment_;

  // Disallow copy and assign
  WebMFile(const WebMFile&);
  WebMFile& operator=(const WebMFile&);
};

}  // namespace webm_dash

#endif  // WEBM_DASH_MANIFEST_WEBM_FILE_H_
