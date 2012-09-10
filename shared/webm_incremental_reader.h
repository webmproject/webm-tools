/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SHARED_WEBM_INCREMENTAL_READER_H_
#define SHARED_WEBM_INCREMENTAL_READER_H_

#include "mkvreader.hpp"
#include "webm_tools_types.h"

namespace webm_tools {

// Provides a moving window into a buffer, and implements libwebm's IMkvReader
// interface.  |WebmBufferParser| sets the window into a buffer by calling
// |SetBufferWindow|, and libwebm parses the data using the |Read| and |Length|
// methods.
class WebmIncrementalReader : public mkvparser::IMkvReader {
 public:
  enum {
    kInvalidArg   = -1,
    kSuccess      = 0,
    kNeedMoreData = 1,
  };
  WebmIncrementalReader();
  virtual ~WebmIncrementalReader();

  // Updates the buffer window and returns |kSuccess|.
  int SetBufferWindow(const uint8* ptr_buffer, int32 length,
                      int64 bytes_consumed);

  // Sets the end of the segment to |position|. |position| is the end of the
  // segment in bytes. The end position may only be set once. Returns true on
  // success.
  bool SetEndOfSegmentPosition(int64 position);

  // IMkvReader methods.
  virtual int Read(int64 read_pos, long length_requested,  // NOLINT
                   uint8* ptr_buf);
  virtual int Length(int64* ptr_total, int64* ptr_available);

 private:
  // Buffer window pointer.
  const uint8* ptr_buffer_;

  // Length of the buffer window to which |ptr_buffer_| provides access.
  int32 window_length_;

  // Sum of all parsed element lengths.
  int64 bytes_consumed_;

  // The end of the segment. -1 represents an unknown segment size.
  int64 end_of_segment_position_;

  WEBM_TOOLS_DISALLOW_COPY_AND_ASSIGN(WebmIncrementalReader);
};

}  // namespace webm_tools

#endif  // SHARED_WEBM_INCREMENTAL_READER_H_
