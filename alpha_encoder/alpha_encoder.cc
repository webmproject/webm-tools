/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This is an example of an VP8 encoder which can produce VP8 videos with alpha.
 * It takes an input file in raw AYUV format, passes it through the encoder, and
 * writes the compressed frames to disk in webm format.
 *
 * The design on how alpha encoding works can be found here: http://goo.gl/wCP1y
 */

#include <cstring>
#include <cstdio>
#include <string>
#include <sstream>

// libwebm parser includes
#include "mkvparser.hpp"
#include "mkvreader.hpp"

// libwebm muxer includes
#include "mkvmuxer.hpp"
#include "mkvmuxerutil.hpp"
#include "mkvwriter.hpp"

using mkvmuxer::uint64;
using mkvmuxer::uint32;
using mkvmuxer::uint8;

namespace {

bool CreateInputFiles(const char* input, int w, int h) {
  FILE* const f_input = fopen(input, "rb");
  if (!f_input) {
    fprintf(stderr, "Failed to open %s for reading", input);
    return false;
  }
  FILE* const f_video = fopen("video.in", "wb");
  if (!f_video) {
    fprintf(stderr, "Failed to open video.in for write");
    return false;
  }
  FILE* const f_alpha = fopen("alpha.in", "wb");
  if (!f_alpha) {
    fprintf(stderr, "Failed to open alpha.in for write");
    return false;
  }
  const int uv_w = (1 + w) / 2;
  const int uv_h = (1 + h) / 2;
  uint8* row = new uint8[w];

  bool eof = false;
  while (!eof) {
    for (int i = 0; i < 6 && !eof; ++i) {
      const uint32 width = (i % 3) ? uv_w : w;
      const uint32 height = (i % 3) ? uv_h : h;
      for (uint32 j = 0; j < height; ++j) {
        if (i < 4) {
          if (fread(row, 1, width, f_input) != width) {
            eof = true;
            break;
          }
        } else {
          memset(row, 0x80, width);
        }
        fwrite(row, 1, width, (i < 3) ? f_video : f_alpha);
      }
    }
  }

  fclose(f_video);
  fclose(f_alpha);
  fclose(f_input);
  delete[] row;

  return true;
}

bool Encode(const std::string& vpxenc_cmd,
            const std::string& vpxenc_options,
            int w, int h) {
  std::ostringstream encode_cmd;
  encode_cmd << vpxenc_cmd << " --width=" << w << " --height=" << h
             << " " << vpxenc_options << " -o video.out video.in";
  fprintf(stderr, "Running %s\n", encode_cmd.str().c_str());
  if (system(encode_cmd.str().c_str()))
    return false;
  encode_cmd.str(std::string());
  encode_cmd << vpxenc_cmd << " --width=" << w << " --height=" << h
             << " " << vpxenc_options << " -o alpha.out alpha.in";
  fprintf(stderr, "Running %s\n", encode_cmd.str().c_str());
  if (system(encode_cmd.str().c_str()))
    return false;
  return true;
}

}  // namespace

void Usage() {
  printf("Usage: alpha_encoder -i input -o output -h height -w width -b "
         "<path_to_vpxenc_binary> [vpxenc_options]\n");
  printf("Options:\n");
  printf("  -? | --help       show help\n");
  printf("  -i                input file (raw yuva420p only)\n");
  printf("  -o                output file (webm with alpha)\n");
  printf("  -w                width of the input file\n");
  printf("  -h                height of the input file\n");
  printf("  -b                absolute/relative path of vpxenc binary. "
         "default is ../../libvpx/vpxenc\n");
  printf(" [vpxenc_options]   options to be passed to vpxenc. these options"
         " are passed on to vpxenc as it is. options to vpxenc should always"
         " be in the end (i.e.) after all the aforementioned options\n");
  printf("\n");
}

bool Init(const char* output,
          mkvmuxer::MkvWriter* writer,
          mkvmuxer::Segment* muxer_segment,
          mkvparser::MkvReader* reader,
          mkvparser::MkvReader* reader_alpha) {
  if (reader->Open("video.out")) {
    fprintf(stderr, "\n Error while opening video.out.\n");
    return false;
  }
  if (reader_alpha->Open("alpha.out")) {
    fprintf(stderr, "\n Error while opening alpha.out.\n");
    return false;
  }
  if (!writer->Open(output)) {
    fprintf(stderr, "\n Error while opening output file.\n");
    return false;
  }
  if (!muxer_segment->Init(writer)) {
    fprintf(stderr, "\n Could not initialize muxer segment!\n");
    return false;
  }
  return true;
}

bool Cleanup(mkvmuxer::MkvWriter* writer,
             mkvparser::MkvReader* reader,
             mkvparser::MkvReader* reader_alpha,
             mkvmuxer::Segment* muxer_segment,
             mkvparser::Segment** parser_segment,
             mkvparser::Segment** parser_segment_alpha) {
  if (!muxer_segment->Finalize())
    fprintf(stderr, "Finalization of segment failed.\n");
  delete *parser_segment;
  delete *parser_segment_alpha;
  writer->Close();
  reader->Close();
  reader_alpha->Close();
  if (remove("video.in") || remove("video.out") ||
      remove("alpha.in") || remove("alpha.out")) {
    fprintf(stderr, "\n Error while removing temp files.\n");
    return false;
  }
  return true;
}

bool WriteTrack(mkvparser::MkvReader* reader,
                mkvparser::MkvReader* reader_alpha,
                mkvmuxer::Segment* muxer_segment,
                mkvparser::Segment** parser_segment,
                mkvparser::Segment** parser_segment_alpha) {
  long long pos = 0;
  mkvparser::EBMLHeader ebml_header;
  ebml_header.Parse(reader, pos);
  long long pos_alpha = 0;
  mkvparser::EBMLHeader ebml_header_alpha;
  ebml_header_alpha.Parse(reader_alpha, pos_alpha);

  long long ret = mkvparser::Segment::CreateInstance(reader,
                                                     pos,
                                                     *parser_segment);
  if (ret) {
    fprintf(stderr, "\n Segment::CreateInstance() failed.");
    return false;
  }
  ret = mkvparser::Segment::CreateInstance(reader_alpha,
                                           pos_alpha,
                                           *parser_segment_alpha);
  if (ret) {
    fprintf(stderr, "\n Segment::CreateInstance() failed.");
    return false;
  }

  ret = (*parser_segment)->Load();
  if (ret < 0) {
    fprintf(stderr, "\n Segment::Load() failed.");
    return false;
  }
  ret = (*parser_segment_alpha)->Load();
  if (ret < 0) {
    fprintf(stderr, "\n Segment::Load() failed.");
    return false;
  }

  const mkvparser::SegmentInfo* const segment_info =
      (*parser_segment)->GetInfo();
  const long long timeCodeScale = segment_info->GetTimeCodeScale();

  muxer_segment->set_mode(mkvmuxer::Segment::kFile);
  mkvmuxer::SegmentInfo* const info = muxer_segment->GetSegmentInfo();
  info->set_timecode_scale(timeCodeScale);
  info->set_writing_app("alpha_encoder");

  const mkvparser::Tracks* const parser_tracks = (*parser_segment)->GetTracks();
  uint64 vid_track = 0;  // no track added

  using mkvparser::Track;

  const mkvparser::Track* const parser_track =
        parser_tracks->GetTrackByIndex(0);
  if (parser_track == NULL)
    return false;

  const mkvparser::VideoTrack* const pVideoTrack =
          static_cast<const mkvparser::VideoTrack*>(parser_track);
  const char* const track_name = pVideoTrack->GetNameAsUTF8();
  const long long width =  pVideoTrack->GetWidth();
  const long long height = pVideoTrack->GetHeight();

  // Add the video track to the muxer
  vid_track = muxer_segment->AddVideoTrack(static_cast<int>(width),
                                          static_cast<int>(height),
                                          1);
  if (!vid_track) {
    fprintf(stderr, "\n Could not add video track.\n");
    return false;
  }

  mkvmuxer::VideoTrack* const video =
      static_cast<mkvmuxer::VideoTrack*>(
          muxer_segment->GetTrackByNumber(vid_track));
  if (!video) {
    fprintf(stderr, "\n Could not get video track.\n");
    return false;
  }

  if (track_name)
    video->set_name(track_name);

  video->SetAlphaMode(1);
  video->set_max_block_additional_id(1);

  const double rate = pVideoTrack->GetFrameRate();
  if (rate > 0.0) {
    video->set_frame_rate(rate);
  }
  return true;
}

bool WriteClusters(mkvparser::MkvReader* reader,
                   mkvparser::MkvReader* reader_alpha,
                   mkvmuxer::Segment* muxer_segment,
                   mkvparser::Segment* parser_segment,
                   mkvparser::Segment* parser_segment_alpha) {
  unsigned char* data = NULL;
  int data_len = 0;
  unsigned char* additional = NULL;
  int additional_len = 0;

  const mkvparser::Cluster* cluster = parser_segment->GetFirst();
  const mkvparser::Cluster* cluster_alpha = parser_segment_alpha->GetFirst();

  while (cluster != NULL && !cluster->EOS() &&
         cluster_alpha != NULL && !cluster_alpha->EOS()) {
    const mkvparser::BlockEntry* block_entry;
    long status = cluster->GetFirst(block_entry);
    if (status) {
      fprintf(stderr, "\n Could not get first block of cluster.\n");
      return false;
    }
    const mkvparser::BlockEntry* block_entry_alpha;
    status = cluster_alpha->GetFirst(block_entry_alpha);
    if (status) {
      fprintf(stderr, "\n Could not get first block of cluster.\n");
      return false;
    }
    while (block_entry != NULL && !block_entry->EOS() &&
           block_entry_alpha != NULL && !block_entry_alpha->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      const mkvparser::Block* const block_alpha =
          block_entry_alpha->GetBlock();
      const long long time_ns = block->GetTime(cluster);
      const int frame_count = block->GetFrameCount();
      const bool is_key = block->IsKey();

      for (int i = 0; i < frame_count; ++i) {
        const mkvparser::Block::Frame& frame = block->GetFrame(i);
        const mkvparser::Block::Frame& frame_alpha = block_alpha->GetFrame(i);

        if (frame.len > data_len) {
          delete [] data;
          data = new unsigned char[frame.len];
          if (!data)
            return false;
          data_len = frame.len;
        }

        if (frame_alpha.len > additional_len) {
          delete [] additional;
          additional = new unsigned char[frame_alpha.len];
          if (!additional)
            return false;
          additional_len = frame_alpha.len;
        }

        if (frame.Read(reader, data))
          return false;

        if (frame_alpha.Read(reader_alpha, additional))
          return false;

        if (!muxer_segment->AddFrameWithAdditional(data,
                                                   frame.len,
                                                   additional,
                                                   frame_alpha.len,
                                                   1, 1,
                                                   time_ns,
                                                   is_key)) {
          fprintf(stderr, "\n Could not add frame.\n");
          return false;
        }
      }
      status = cluster->GetNext(block_entry, block_entry);
      if (status) {
        fprintf(stderr, "\n Could not get next block of cluster.\n");
        return false;
      }
      status = cluster_alpha->GetNext(block_entry_alpha, block_entry_alpha);
      if (status) {
        fprintf(stderr, "\n Could not get next block of cluster.\n");
        return false;
      }
    }
    cluster = parser_segment->GetNext(cluster);
    cluster_alpha = parser_segment_alpha->GetNext(cluster_alpha);
  }
  delete[] data;
  delete[] additional;
  return true;
}

int main(int argc, char** argv) {
  const char* input = NULL;
  const char* output = NULL;
  std::string vpxenc_cmd = "../../libvpx/vpxenc";
  std::string vpxenc_options = "";
  long w = -1;
  long h = -1;
  int i;

  // Parse command line parameters
  const int argc_check = argc - 1;
  for (i = 1; i < argc; ++i) {
    char* end;
    if (!strcmp("-?", argv[i]) || !strcmp("--help", argv[i])) {
      Usage();
      return EXIT_SUCCESS;
    } else if (!strcmp("-i", argv[i]) && i < argc_check) {
      input = argv[++i];
    } else if (!strcmp("-o", argv[i]) && i < argc_check) {
      output = argv[++i];
    } else if (!strcmp("-w", argv[i]) && i < argc_check) {
      w = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-h", argv[i]) && i < argc_check) {
      h = strtol(argv[++i], &end, 10);
    } else if (!strcmp("-b", argv[i]) && i < argc_check) {
      vpxenc_cmd = argv[++i];
    } else {
      break;
    }
  }
  // All remaining parameters are to be passed on to vpxenc
  for (; i < argc; ++i) {
    vpxenc_options += argv[i];
    vpxenc_options += " ";
  }

  // Validate input parameters
  if (input == NULL || output == NULL || w == -1 || h == -1) {
    Usage();
    return EXIT_FAILURE;
  }
  if (w < 16 || h < 16) {
    fprintf(stderr, "Invalid resolution: %ldx%ld", w, h);
    return EXIT_FAILURE;
  }

  mkvparser::MkvReader reader;
  mkvparser::MkvReader reader_alpha;
  mkvmuxer::MkvWriter writer;
  mkvmuxer::Segment muxer_segment;
  mkvparser::Segment* parser_segment = NULL;
  mkvparser::Segment* parser_segment_alpha = NULL;

  if (!CreateInputFiles(input, w, h) ||
      !Encode(vpxenc_cmd, vpxenc_options, w, h) ||
      !Init(output, &writer, &muxer_segment, &reader, &reader_alpha) ||
      !WriteTrack(&reader, &reader_alpha, &muxer_segment,
                  &parser_segment, &parser_segment_alpha) ||
      !WriteClusters(&reader, &reader_alpha, &muxer_segment,
                     parser_segment, parser_segment_alpha) ||
      !Cleanup(&writer, &reader, &reader_alpha, &muxer_segment,
               &parser_segment, &parser_segment_alpha))
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
