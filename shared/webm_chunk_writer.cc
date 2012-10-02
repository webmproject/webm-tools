// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webm_chunk_writer.h"

#include <cstdio>
#include <vector>

#include "mkvmuxer.hpp"
#include "webmids.hpp"

namespace webm_tools {

WebMChunkWriter::WebMChunkWriter()
    : bytes_buffered_(0),
      bytes_written_(0),
      chunk_end_(0),
      ptr_write_buffer_(NULL) {
}

WebMChunkWriter::~WebMChunkWriter() {
}

int32 WebMChunkWriter::Init(WriteBuffer* ptr_write_buffer) {
  if (!ptr_write_buffer) {
    fprintf(stderr, "Cannot Init, NULL write buffer.\n");
    return kInvalidArg;
  }
  ptr_write_buffer_ = ptr_write_buffer;
  return kSuccess;
}

void WebMChunkWriter::EraseChunk() {
  if (ptr_write_buffer_) {
    WriteBuffer::iterator erase_end_pos =
        ptr_write_buffer_->begin() + static_cast<int32>(chunk_end_);
    ptr_write_buffer_->erase(ptr_write_buffer_->begin(), erase_end_pos);
    bytes_buffered_ = ptr_write_buffer_->size();
    chunk_end_ = 0;
  }
}

int32 WebMChunkWriter::Write(const void* ptr_buffer, uint32 buffer_length) {
  if (!ptr_write_buffer_) {
    fprintf(stderr, "Cannot Write, not Initialized.\n");
    return kNotInitialized;
  }
  if (!ptr_buffer || !buffer_length) {
    fprintf(stderr, "Error invalid arg passed to Write.\n");
    return kInvalidArg;
  }
  const uint8* ptr_data = reinterpret_cast<const uint8*>(ptr_buffer);
  ptr_write_buffer_->insert(ptr_write_buffer_->end(),
                            ptr_data,
                            ptr_data + buffer_length);
  bytes_written_ += buffer_length;
  bytes_buffered_ = ptr_write_buffer_->size();
  return kSuccess;
}

void WebMChunkWriter::ElementStartNotify(uint64 element_id, int64 position) {
  if (element_id == mkvmuxer::kMkvCluster) {
    chunk_end_ = bytes_buffered_;
    fprintf(stdout, "chunk_end_=%lld position=%lld\n", chunk_end_, position);
  }
}

}  // namespace webm_tools
