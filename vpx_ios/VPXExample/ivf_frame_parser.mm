// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "./ivf_frame_parser.h"

#import <CoreFoundation/CoreFoundation.h>

#include <string>
#include <vector>

namespace VpxExample {

namespace {
const int kIvfFormatVersion = 0;

// IVF file "signature" string.
const char *kIvfSignature = "DKIF";

// IVF file size constants.
const int kIvfSignatureSize = 4;
const int kIvfFileHeaderSize = 32;
const int kIvfFrameHeaderSize = 12;

// IVF file header field offset constants.
// Offsets taken from http://wiki.multimedia.cx/index.php?title=IVF
// Offsets are in position order. The IVF signature offset, which is
// 0, has been omitted. Field value size is inferred by the difference between
// each offset, with the exception of the frame count, the size of which is 4.
const int kIvfVersionOffset = 4;
const int kIvfHeaderSizeOffset = 6;
const int kIvfCodecFourCcOffset = 8;
const int kIvfWidthOffset = 12;
const int kIvfHeightOffset = 14;
const int kIvfRateOffset = 16;
const int kIvfScaleOffset = 20;
const int kIvfFrameCountOffset = 24;

// IVF frame header offset constants.
const int kIvfFrameSizeOffset = 0;

// VPx four CC constants.
const uint32_t kVp8FourCc = 0x30385056;
const uint32_t kVp9FourCc = 0x30395056;

uint16_t ReadIvfHeaderUint16(const std::string &ivf_header, int offset) {
  const void *read_head = reinterpret_cast<const void *>(&ivf_header[offset]);
  return CFSwapInt16LittleToHost(*reinterpret_cast<const uint16_t*>(read_head));
}

uint32_t ReadIvfHeaderUint32(const std::string &ivf_header, int offset) {
  const void *read_head = reinterpret_cast<const void *>(&ivf_header[offset]);
  return CFSwapInt32LittleToHost(*reinterpret_cast<const uint32_t*>(read_head));
}
}  // namespace

void IvfFrameParser::FileCloser::operator()(FILE* file) {
  fclose(file);
}

// Processes the 32-byte IVF file header and returns true when the IVF file
// appears to contain VPx video.
bool IvfFrameParser::HasVpxFrames(const std::string &file_path,
                                  VpxFormat *vpx_format) {
  file_.reset(fopen(file_path.c_str(), "rb"));
  if (!file_.get()) {
    NSLog(@"Unable to open %s", file_path.c_str());
    return false;
  }

  std::string ivf_header;
  ivf_header.reserve(kIvfFileHeaderSize);

  const size_t read_count = fread(reinterpret_cast<void*>(&ivf_header[0]),
                                  1,
                                  kIvfFileHeaderSize,
                                  file_.get());
  if (read_count != kIvfFileHeaderSize) {
    NSLog(@"Read failure after %zu bytes", read_count);
    return false;
  }

  const void *read_head = reinterpret_cast<const void*>(&ivf_header[0]);
  if (memcmp(read_head, kIvfSignature, kIvfSignatureSize) != 0) {
    NSLog(@"%s is not an IVF file.", file_path.c_str());
    return false;
  }

  const uint16_t version = ReadIvfHeaderUint16(ivf_header, kIvfVersionOffset);
  if (version != kIvfFormatVersion) {
    NSLog(@"%s has an invalid IVF file version (%hu).",
          file_path.c_str(), version);
    return false;
  }

  const uint16_t header_size = ReadIvfHeaderUint16(ivf_header,
                                                   kIvfHeaderSizeOffset);
  if (header_size != kIvfFileHeaderSize) {
    NSLog(@"%s has an invalid IVF header size (%hu).",
          file_path.c_str(), header_size);
    return false;
  }

  const uint32_t fourcc = ReadIvfHeaderUint32(ivf_header,
                                              kIvfCodecFourCcOffset);
  if (fourcc == kVp8FourCc) {
    vpx_format_.codec = VP8;
  } else if (fourcc == kVp9FourCc) {
    vpx_format_.codec = VP9;
  } else {
    NSLog(@"Invalid codec four CC (%4s) in %s",
          reinterpret_cast<const char *>(&fourcc), file_path.c_str());
    return false;
  }

  vpx_format_.width = ReadIvfHeaderUint16(ivf_header, kIvfWidthOffset);
  vpx_format_.height = ReadIvfHeaderUint16(ivf_header, kIvfHeightOffset);
  *vpx_format = vpx_format_;

  // TODO(tomfinegan): Do something with this data?
  rate_ = ReadIvfHeaderUint32(ivf_header, kIvfRateOffset);
  scale_ = ReadIvfHeaderUint32(ivf_header, kIvfScaleOffset);
  frame_count_ = ReadIvfHeaderUint32(ivf_header, kIvfFrameCountOffset);

  return true;
}

bool IvfFrameParser::ReadFrame(std::vector<uint8_t> *frame,
                               uint32_t *frame_length) {
  if (!frame || !frame_length) {
    return false;
  }

  std::string ivf_frame_header;
  ivf_frame_header.reserve(kIvfFrameHeaderSize);

  size_t read_count = fread(reinterpret_cast<void*>(&ivf_frame_header[0]),
                            1,
                            kIvfFrameHeaderSize,
                            file_.get());
  if (feof(file_.get())) {
    NSLog(@"End of file.");
    return false;
  }
  if (read_count != kIvfFrameHeaderSize) {
    NSLog(@"Read failure after %zu frame header bytes", read_count);
    return false;
  }

  const uint32_t frame_payload_size =
      ReadIvfHeaderUint32(ivf_frame_header, kIvfFrameSizeOffset);

  if (frame->capacity() < frame_payload_size) {
    frame->reserve(frame_payload_size * 2);
  }

  read_count = fread(reinterpret_cast<void*>(&(*frame)[0]),
                     1,
                     frame_payload_size,
                     file_.get());
  if (read_count != frame_payload_size) {
    NSLog(@"Read failure, file truncated? (expected: %u got: %zu)",
          frame_payload_size, read_count);
  }

  *frame_length = frame_payload_size;
  return true;
}

}  // namespace VpxExample
