// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef SHARED_WEBM_CHUNK_WRITER_H_
#define SHARED_WEBM_CHUNK_WRITER_H_

#include <vector>

#include "mkvmuxer.hpp"
#include "webm_tools_types.h"

namespace webm_tools {

// Buffer object implementing libwebm's IMkvWriter interface. Uses
// |WriteBuffer| passed to |Init()| to store data written by libwebm.
class WebMChunkWriter : public mkvmuxer::IMkvWriter {
 public:
  typedef std::vector<uint8> WriteBuffer;

  enum {
    kNotImplemented = -200,
    kNotInitialized = -2,
    kInvalidArg = -1,
    kSuccess = 0,
  };
  WebMChunkWriter();
  virtual ~WebMChunkWriter();

  // Stores |ptr_buffer| and returns |kSuccess|.
  int32 Init(WriteBuffer* ptr_write_buffer);

  // Accessors.
  int64 bytes_written() const { return bytes_written_; }
  int64 chunk_end() const { return chunk_end_; }

  // Erases chunk from |ptr_write_buffer_|, resets |chunk_end_| to 0, and
  // updates |bytes_buffered_|.
  void EraseChunk();

  // mkvmuxer::IMkvWriter methods
  // Returns total bytes of data passed to |Write|.
  virtual int64 Position() const { return bytes_written_; }

  // Not seekable, return |kNotImplemented| on seek attempts.
  virtual int32 Position(int64) { return kNotImplemented; }  // NOLINT

  // Always returns false: |WebMChunkWriter| is never seekable. Written data
  // goes into a vector, and data is buffered only until a chunk is completed.
  virtual bool Seekable() const { return false; }

  // Writes |ptr_buffer| contents to |ptr_write_buffer_|.
  virtual int32 Write(const void* ptr_buffer, uint32 buffer_length);

  // Called by libwebm, and notifies writer of element start position.
  virtual void ElementStartNotify(uint64 element_id, int64 position);

 private:
  int64 bytes_buffered_;
  int64 bytes_written_;
  int64 chunk_end_;
  WriteBuffer* ptr_write_buffer_;
  WEBM_TOOLS_DISALLOW_COPY_AND_ASSIGN(WebMChunkWriter);
};

}  // namespace webm_tools

#endif  // SHARED_WEBM_CHUNK_WRITER_H_
