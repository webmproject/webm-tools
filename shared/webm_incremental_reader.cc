/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webm_incremental_reader.h"

#include <cstdio>
#include <cstring>

#include "mkvreader.hpp"

namespace webm_tools {

WebmIncrementalReader::WebmIncrementalReader()
    : ptr_buffer_(NULL),
      window_length_(0),
      bytes_consumed_(0),
      end_of_segment_position_(-1) {
}

WebmIncrementalReader::~WebmIncrementalReader() {
}

int WebmIncrementalReader::SetBufferWindow(const uint8* ptr_buffer,
                                           int32 length,
                                           int64 bytes_consumed) {
  if (!ptr_buffer || !length) {
    fprintf(stderr, "Invalid arg(s)\n");
    return kInvalidArg;
  }
  ptr_buffer_ = ptr_buffer;
  window_length_ = length;
  bytes_consumed_ = bytes_consumed;
  return kSuccess;
}

bool WebmIncrementalReader::SetEndOfSegmentPosition(int64 offset) {
  if (end_of_segment_position_ != -1)
    return false;

  end_of_segment_position_ = offset;
  return true;
}

int WebmIncrementalReader::Read(int64 read_pos,
                                long length_requested,  // NOLINT
                                uint8* ptr_buf) {
  if (!ptr_buf) {
    fprintf(stderr, "NULL ptr_buf\n");
    return 0;
  }

  // |read_pos| includes |bytes_consumed_|, which is not going to work with the
  // buffer window-- calculate actual offset within |ptr_buf|.
  const int64 window_pos = read_pos - bytes_consumed_;
  if (window_pos < 0) {
    fprintf(stderr, "Error bad window_pos:%lld  read_pos:%lld\n",
            window_pos, read_pos);
    return 0;
  }

  // Is enough data in the buffer?
  const int64 bytes_available = window_length_ - window_pos;
  if (bytes_available < length_requested) {
    // No, not enough data buffered.
    return mkvparser::E_BUFFER_NOT_FULL;
  }

  // Yes, there's enough data in the buffer.
  memcpy(ptr_buf, ptr_buffer_ + window_pos, length_requested);
  return kSuccess;
}

// Called by libwebm to obtain length of readable data.  Returns
// |bytes_consumed_| + |length_| as available amount, which is the size of the
// buffer current window plus the sum of the sizes of all parsed elements.
int WebmIncrementalReader::Length(int64* ptr_total, int64* ptr_available) {
  if (!ptr_total || !ptr_available) {
    fprintf(stderr, "Invalid arg(s)\n");
    return -1;
  }
  *ptr_total = end_of_segment_position_;
  *ptr_available = bytes_consumed_ + window_length_;
  return 0;
}

}  // namespace webm_tools
