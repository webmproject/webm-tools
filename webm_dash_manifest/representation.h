/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBM_DASH_MANIFEST_REPRESENTATION_H_
#define WEBM_DASH_MANIFEST_REPRESENTATION_H_

#include <memory>
#include <string>

#include "webm_tools_types.h"

namespace webm_tools {
class Indent;
}  // namespace webm_tools

namespace mkvparser {
class AudioTrack;
class Cues;
struct EBMLHeader;
class MkvReader;
class Segment;
class Track;
class VideoTrack;
}  // namespace mkvparser

namespace webm_dash {

class DashModel;
class WebMFile;

class Representation {
 public:
  Representation(const std::string& id, const DashModel& dash);
  ~Representation();

  // Finds the WebM file from the dash model. Returns true if the file has
  // been found and set.
  bool SetWebMFile();

  // Returns true if the TrackNumber, CodecID, and CodecPrivate values match
  // the values in |representation|.
  bool BitstreamSwitching(const Representation& representation) const;

  // Returns true if the start time and the block number of all the cue
  // points in the representation are equal to all of the cue points in
  // |representation|.
  bool CheckCuesAlignement(const Representation& representation) const;

  // Returns the sample rate in the first audio track. Returns 0 if there is
  // no audio track.
  int GetAudioSampleRate() const;

  // Returns the average framerate of the first video track. Returns 0.0 if
  // there is no video track or there is no FrameRate element.
  double GetVideoFramerate() const;

  // Returns the height in pixels of the first video track. Returns 0 if there
  // is no video track.
  int GetVideoHeight() const;

  // Returns the width in pixels of the first video track. Returns 0 if there
  // is no video track.
  int GetVideoWidth() const;

  // Outputs Representation in the Dash format. Returns true if there were no
  // errors with the output.
  bool OutputDashManifest(FILE* o, webm_tools::Indent* indent) const;

  // Check all the subsegments within the Representation to see if they
  // conform to the subsegmentStartsWithSAP attribute.
  bool SubsegmentStartsWithSAP() const;

  std::string id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  void set_output_audio_sample_rate(bool output) {
    output_audio_sample_rate_ = output;
  }

  bool output_header() const { return output_header_; }
  void set_output_header(bool output) { output_header_ = output; }

  bool output_index() const { return output_index_; }
  void set_output_index(bool output) { output_index_ = output; }

  void set_output_video_height(bool output) { output_video_height_ = output; }

  void set_output_video_width(bool output) { output_video_width_ = output; }

  const WebMFile* webm_file() const { return webm_file_; }

  std::string webm_filename() const { return webm_filename_; }
  void set_webm_filename(const std::string& file) { webm_filename_ = file; }

 private:
  // Outputs SegmentBase in the Dash format. Returns true if there were no
  // errors with the output.
  bool OutputSegmentBase(FILE* o, webm_tools::Indent* indent) const;

  // The main class for the manifest.
  const DashModel& dash_model_;

  // TODO(fgalligan): Add code to make sure id is unique within the Period.
  // Used to differentiate between different representation within an
  // adaptation set.
  std::string id_;

  // Flag telling if the class should output audio sample rate.
  bool output_audio_sample_rate_;

  // Flag telling if the class should output the header information.
  bool output_header_;

  // Flag telling if the class should output the header information.
  bool output_index_;

  // Flag telling if the class should output video height.
  bool output_video_height_;

  // Flag telling if the class should output video width.
  bool output_video_width_;

  // WebM file. This class does not own the pointer.
  const WebMFile* webm_file_;

  // Path to WebM file.
  std::string webm_filename_;

  WEBM_TOOLS_DISALLOW_COPY_AND_ASSIGN(Representation);
};

}  // namespace webm_dash

#endif  // WEBM_DASH_MANIFEST_REPRESENTATION_H_
