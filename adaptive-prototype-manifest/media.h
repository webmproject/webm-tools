/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef MEDIA_H_
#define MEDIA_H_

#include <memory>
#include <string>

namespace indent_webm {
class Indent;
}

namespace mkvparser {
class AudioTrack;
class Cues;
struct EBMLHeader;
class MkvReader;
class Segment;
class Track;
class VideoTrack;
}

namespace adaptive_manifest {

using std::string;

class Media {
 public:
  explicit Media(const string& id);
  ~Media();

  // Load and parse the webm file. Checks to make sure there is only an audio
  // or video stream and the stream matches the codecs defined in WebM.
  // Returns true if the file has been loaded and verified.
  bool Init();

  // Returns true if the start time and the block number of all the cue
  // points in the media are equal to all of the cue points in |media|.
  bool CheckAlignement(const Media& media);

  // Returns the codec string associated with the first track. If the CodecID
  // is V_VP8 then the string returned will be "vp8". If the CodecID is
  // A_VORBIS then the string returned will be "vorbis". If the CodecID is
  // anything else then the string returned will be empty.
  string GetCodec() const;

  // Returns the Cues from the media.
  const mkvparser::Cues* GetCues() const;

  // Returns the duration of the file in nanoseconds.
  long long GetDurationNanoseconds() const;

  // Outputs Media in the prototype format.
  void OutputPrototypeManifest(std::ostream& o, indent_webm::Indent& indt);

  const string& id() {return id_;}
  void id(const string& id) {id_ = id;}

  const string& file() {return file_;}
  void file(const string& file) {file_ = file;}

 private:
  // Returns true if the first four bytes of the doctype of the WebM file
  // match "webm".
  bool CheckDocType() const;

  // Returns true if the first video track equals V_VP8 or the first audio
  // track equals A_VORBIS. Returns false if there are no audio or video
  // tracks. Returns false if there is both a video tack and an audio track.
  // This is because for adaptive streaming we want the streams to be separate.
  bool CheckCodecTypes() const;

  // Returns true if the file has a Cues element.
  bool CheckForCues() const;

  // Returns the |start| and |end| byte offsets and the start and end times
  // of the requested chunk. |start_time_nano| is the time in nano seconds
  // inclusive to start searching for in the Cues element. |end_time_nano| is
  // the time in nano seconds exclusive to end searching for in the Cues
  // element. If you want to get the entries Cues element set |end_time_nano|
  // to max of long long.
  void FindCuesChunk(long long start_time_nano,
                     long long end_time_nano,
                     long long& start,
                     long long& end,
                     long long& cue_start_time,
                     long long& cue_end_time);

  // Returns the number of channels in the first audio track. Returns 0 if
  // there is no audio track.
  int GetAudioChannels() const;

  // Returns the sample rate in the first audio track. Returns 0 if there is
  // no audio track.
  int GetAudioSampleRate() const;

  // Return the first audio track. Returns NULL if there are no audio tracks.
  const mkvparser::AudioTrack* GetAudioTrack() const;

  // Calculate and return average bandwidth for the WebM file in kilobits
  // per second.
  long long GetAverageBandwidth() const;

  // Returns the byte offset in the file for the start of the first Cluster
  // element starting with the EBML element ID. A value of -1 indicates there
  // was an error.
  long long GetClusterRangeStart() const;

  // Returns the byte offset in the file for the start of the SegmentInfo
  // element starting with the EBML element ID to the end offset of the
  // element.
  void GetSegmentInfoRange(long long& start, long long& end) const;

  // Returns the Track by an index. Returns NULL if it cannot find the track
  // represented by |index|.
  const mkvparser::Track* GetTrack(unsigned int index) const;

  // Returns the byte offset in the file for the start of the Tracks element
  // starting with the EBML element ID to the end offset of the element.
  void GetTracksRange(long long& start, long long& end) const;

  // Returns the byte offset in the file for the start of the Segment Info and
  // Tracks element starting with the EBML element ID to the end offset of the
  // element. A return value of -1 for either value indicates an error.
  void GetHeaderRange(long long& start, long long& end) const;

  // Returns the average framerate of the first video track. Returns 0.0 if
  // there is no video track or we cannot generate an average framerate.
  double GetVideoFramerate() const;

  // Returns the height in pixels of the first video track. Returns 0 if there
  // is no video track.
  int GetVideoHeight() const;

  // Returns the width in pixels of the first video track. Returns 0 if there
  // is no video track.
  int GetVideoWidth() const;

  // Return the first video track. Returns NULL if there are no video tracks.
  const mkvparser::VideoTrack* GetVideoTrack() const;

  // Outputs MediaHeader in the prototype format. The byte range will include
  // the Info and the Tracks elements.
  void OutputPrototypeManifestMediaHeader(std::ostream& o,
                                          indent_webm::Indent& indt);

  // Outputs MediaIndex in the prototype format. The byte range will include
  // the Cues element.
  void OutputPrototypeManifestMediaIndex(std::ostream& o,
                                         indent_webm::Indent& indt);

  // Outputs Cues element in the prototype format.
  void OutputPrototypeManifestCues(std::ostream& o, indent_webm::Indent& indt);

  friend std::ostream& operator<< (std::ostream &o, const Media &m);

  // Time in nano seconds to split up the Cues element into the chunkindexlist.
  long long cue_chunk_time_nano_;

  // Used to differentiate between different media within a media group.
  string id_;

  // Path to WebM file.
  string file_;

  // TODO: Change this from auto_ptr. Should we use boost?
  // EBML header of the file.
  std::auto_ptr<mkvparser::EBMLHeader> ebml_header_;

  // libwebm reader
  std::auto_ptr<mkvparser::MkvReader> reader_;

  // Main WebM element.
  std::auto_ptr<mkvparser::Segment> segment_;

  // Disallow copy and assign
  Media(const Media&);
  Media& operator=(const Media&);
};

}  // namespace adaptive_manifest

#endif  // MEDIA_H_
