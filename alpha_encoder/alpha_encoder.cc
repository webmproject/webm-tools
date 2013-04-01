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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// libwebm parser includes
#include "mkvreader.hpp"
#include "mkvparser.hpp"

// libwebm muxer includes
#include "mkvmuxer.hpp"
#include "mkvwriter.hpp"
#include "mkvmuxerutil.hpp"

using mkvmuxer::uint64;

namespace {

int create_input_files(char *input, int w, int h) {
  FILE *f_input;
  FILE *f_video;
  FILE *f_alpha;
  if (!(f_input = fopen(input, "rb"))) {
    printf("Failed to open video.in for write");
    return EXIT_FAILURE;
  }
  if (!(f_video = fopen("video.in", "w"))) {
    printf("Failed to open video.in for write");
    return EXIT_FAILURE;
  }
  if (!(f_alpha = fopen("alpha.in", "w"))) {
    printf("Failed to open alpha.in for write");
    return EXIT_FAILURE;
  }
  const int uv_w = (1 + w) / 2;
  const int uv_h = (1 + h) / 2;
  unsigned char *planes[6];
  int i;

  for (i = 0; i < 6; i++)
    planes[i] = new unsigned char[(i == 0 || i == 3) ? w : uv_w];

  // TODO(vigneshv): Should this be 0 or something else ?
  memset(planes[4], 0, uv_w);
  memset(planes[5], 0, uv_w);

  bool eof = false;
  while (!eof) {
    for (i = 0; i < 6 && !eof; i++) {
      const unsigned int width = (i == 0 || i == 3) ? w : uv_w;
      const unsigned int height = (i == 0 || i == 3) ? h : uv_h;
      for (unsigned int row = 0; row < height; row++) {
        if (i < 4) {
          if (fread(planes[i], 1, width, f_input) != width) {
            eof = true;
            break;
          }
        }
        fwrite(planes[i], 1, width, (i < 3) ? f_video : f_alpha);
      }
    }
  }

  fclose(f_video);
  fclose(f_alpha);
  fclose(f_input);

  for (i = 0; i < 6; i++)
    delete planes[i];

  return 1;
}

void encode(int w, int h, char *vpxenc_cmd, char* vpxenc_options) {
  char encode_cmd[10000];
  sprintf(encode_cmd,
          "%s --quiet --width=%d --height=%d %s -o video.out video.in",
          vpxenc_cmd, w, h, vpxenc_options);
  fprintf(stderr, "Running %s\n", encode_cmd);
  if (system(encode_cmd))
    exit(EXIT_FAILURE);
  sprintf(encode_cmd,
          "%s --quiet --width=%d --height=%d %s -o alpha.out alpha.in",
          vpxenc_cmd, w, h, vpxenc_options);
  fprintf(stderr, "Running %s\n", encode_cmd);
  if (system(encode_cmd))
    exit(EXIT_FAILURE);
}

}  // end anonymous namespace

void Usage() {
  printf("Usage: alpha_encoder -i input -o output -h height -w width -v vpexenc_cmd [vpxenc_options]\n");
  printf("Options:\n");
  printf("  -? | --help       show help\n");
  printf("  -i                input file (raw yuva420p only)\n");
  printf("  -o                output file (webm with alpha)\n");
  printf("  -w                width of the input file\n");
  printf("  -h                height of the input file\n");
  printf("  -v                absolute/relative path of vpxenc binary. "
         "default is ../../libvpx/vpxenc\n");
  printf(" [vpxenc_options]   options to be passed to vpxenc. these options"
         " are passed on to vpxenc as it is. options to vpxenc should always"
         " be in the end (i.e.) after all the aforementioned options\n");
  printf("\n");
}

int main(int argc, char **argv) {
  char* input = NULL;
  char* output = NULL;
  char* vpxenc_cmd = "../../libvpx/vpxenc";
  char vpxenc_options[1000];
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
    } else if (!strcmp("-v", argv[i]) && i < argc_check) {
      vpxenc_cmd = argv[++i];
    } else {
      break;
    }
  }
  // All remaining parameters are to be passed on to vpxenc
  strncpy(vpxenc_options, "", 1);
  for (; i < argc; ++i) {
    strncat(vpxenc_options, argv[i], strlen(argv[i]));
    strncat(vpxenc_options, " ", 1);
  }

  // Validate input parameters
  if (input == NULL || output == NULL || w == -1 || h == -1) {
    Usage();
    return EXIT_FAILURE;
  }
  if (w < 16 || h < 16) {
    printf("Invalid resolution: %ldx%ld", w, h);
    return EXIT_FAILURE;
  }

  create_input_files(input, w, h);
  encode(w, h, vpxenc_cmd, vpxenc_options);

  // Get parser header info
  mkvparser::MkvReader reader;
  if (reader.Open("video.out")) {
    printf("\n Filename is invalid or error while opening video.out.\n");
    return EXIT_FAILURE;
  }
  mkvparser::MkvReader reader_alpha;
  if (reader_alpha.Open("alpha.out")) {
    printf("\n Filename is invalid or error while opening alpha.out.\n");
    return EXIT_FAILURE;
  }

  long long pos = 0;
  mkvparser::EBMLHeader ebml_header;
  ebml_header.Parse(&reader, pos);
  long long pos_alpha = 0;
  mkvparser::EBMLHeader ebml_header_alpha;
  ebml_header_alpha.Parse(&reader_alpha, pos_alpha);

  mkvparser::Segment* parser_segment;
  long long ret = mkvparser::Segment::CreateInstance(&reader,
                                                     pos,
                                                     parser_segment);
  if (ret) {
    printf("\n Segment::CreateInstance() failed.");
    return EXIT_FAILURE;
  }
  mkvparser::Segment* parser_segment_alpha;
  ret = mkvparser::Segment::CreateInstance(&reader_alpha,
                                           pos_alpha,
                                           parser_segment_alpha);
  if (ret) {
    printf("\n Segment::CreateInstance() failed.");
    return EXIT_FAILURE;
  }

  ret = parser_segment->Load();
  if (ret < 0) {
    printf("\n Segment::Load() failed.");
    return EXIT_FAILURE;
  }
  ret = parser_segment_alpha->Load();
  if (ret < 0) {
    printf("\n Segment::Load() failed.");
    return EXIT_FAILURE;
  }

  const mkvparser::SegmentInfo* const segment_info = parser_segment->GetInfo();
  const long long timeCodeScale = segment_info->GetTimeCodeScale();

  mkvmuxer::MkvWriter writer;
  if (!writer.Open(output)) {
    printf("\n Filename is invalid or error while opening output file.\n");
    return EXIT_FAILURE;
  }

  mkvmuxer::Segment muxer_segment;
  if (!muxer_segment.Init(&writer)) {
    printf("\n Could not initialize muxer segment!\n");
    return EXIT_FAILURE;
  }
  muxer_segment.set_mode(mkvmuxer::Segment::kFile);
  mkvmuxer::SegmentInfo* const info = muxer_segment.GetSegmentInfo();
  info->set_timecode_scale(timeCodeScale);
  info->set_writing_app("alpha_encoder");

  const mkvparser::Tracks* const parser_tracks = parser_segment->GetTracks();
  uint64 vid_track = 0;  // no track added

  using mkvparser::Track;

  const mkvparser::Track* const parser_track =
        parser_tracks->GetTrackByIndex(0);
  if (parser_track == NULL)
    return EXIT_FAILURE;

  const mkvparser::VideoTrack* const pVideoTrack =
          static_cast<const mkvparser::VideoTrack*>(parser_track);
  const char* const track_name = pVideoTrack->GetNameAsUTF8();
  const long long width =  pVideoTrack->GetWidth();
  const long long height = pVideoTrack->GetHeight();

  // Add the video track to the muxer
  vid_track = muxer_segment.AddVideoTrack(static_cast<int>(width),
                                          static_cast<int>(height),
                                          1);
  if (!vid_track) {
    printf("\n Could not add video track.\n");
    return EXIT_FAILURE;
  }

  mkvmuxer::VideoTrack* const video =
      static_cast<mkvmuxer::VideoTrack*>(
          muxer_segment.GetTrackByNumber(vid_track));
  if (!video) {
    printf("\n Could not get video track.\n");
    return EXIT_FAILURE;
  }

  if (track_name)
    video->set_name(track_name);

  video->SetAlphaMode(1);
  video->set_max_block_additional_id(1);

  const double rate = pVideoTrack->GetFrameRate();
  if (rate > 0.0) {
    video->set_frame_rate(rate);
  }

  // Write clusters
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
        printf("\n Could not get first block of cluster.\n");
        return EXIT_FAILURE;
    }
    const mkvparser::BlockEntry* block_entry_alpha;
    status = cluster_alpha->GetFirst(block_entry_alpha);
    if (status) {
        printf("\n Could not get first block of cluster.\n");
        return EXIT_FAILURE;
    }
    while (block_entry != NULL && !block_entry->EOS() &&
           block_entry_alpha != NULL && !block_entry_alpha->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      const mkvparser::Block* const block_alpha =
          block_entry_alpha->GetBlock();
      const mkvparser::Track* const parser_track =
          parser_tracks->GetTrackByNumber(
              static_cast<unsigned long>(1));
      if (parser_track == NULL) {
        printf("\n Could not find Video Track.\n");
        return EXIT_FAILURE;
      }
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
            return EXIT_FAILURE;
          data_len = frame.len;
        }

        if (frame_alpha.len > additional_len) {
          delete [] additional;
          additional = new unsigned char[frame_alpha.len];
          if (!additional)
            return EXIT_FAILURE;
          additional_len = frame_alpha.len;
        }

        if (frame.Read(&reader, data))
          return EXIT_FAILURE;

        if (frame_alpha.Read(&reader_alpha, additional))
          return EXIT_FAILURE;

        if (!muxer_segment.AddFrameWithAdditional(data,
                                                  frame.len,
                                                  additional,
                                                  frame_alpha.len,
                                                  1, 1,
                                                  time_ns,
                                                  is_key)) {
          printf("\n Could not add frame.\n");
          return EXIT_FAILURE;
        }
      }
      status = cluster->GetNext(block_entry, block_entry);
      if (status) {
          printf("\n Could not get next block of cluster.\n");
          return EXIT_FAILURE;
      }
      status = cluster_alpha->GetNext(block_entry_alpha, block_entry_alpha);
      if (status) {
          printf("\n Could not get next block of cluster.\n");
          return EXIT_FAILURE;
      }
    }
    cluster = parser_segment->GetNext(cluster);
    cluster_alpha = parser_segment_alpha->GetNext(cluster_alpha);
  }

  if (!muxer_segment.Finalize()) {
    printf("Finalization of segment failed.\n");
    return EXIT_FAILURE;
  }

  delete[] data;
  delete[] additional;
  delete parser_segment;
  delete parser_segment_alpha;

  writer.Close();
  reader.Close();
  reader_alpha.Close();

  if(remove("video.in") || remove("video.out") ||
     remove("alpha.in") || remove("alpha.out")) {
    printf("\n Error while removing temp files.\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
