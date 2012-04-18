// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "indent.h"
#include "mkvreader.hpp"
#include "mkvparser.hpp"
#include "webm_endian.h"

namespace {

using mkvparser::ContentEncoding;
using std::string;
using std::wstring;
using webm_tools::Indent;
using webm_tools::int64;
using webm_tools::uint8;
using webm_tools::uint64;
using webm_tools::kNanosecondsPerSecond;

const char VERSION_STRING[] = "1.0.1.0";

struct Options {
  Options();

  // Set all of the member variables to |value|.
  void SetAll(bool value);

  bool output_video;
  bool output_audio;
  bool output_size;
  bool output_offset;
  bool output_seconds;
  bool output_ebml_header;
  bool output_segment;
  bool output_segment_info;
  bool output_tracks;
  bool output_clusters;
  bool output_blocks;
  bool output_vp8_info;
  bool output_clusters_size;
  bool output_encrypted_info;
};

Options::Options()
    : output_video(true),
      output_audio(true),
      output_size(false),
      output_offset(false),
      output_seconds(true),
      output_ebml_header(true),
      output_segment(true),
      output_segment_info(true),
      output_tracks(true),
      output_clusters(false),
      output_blocks(false),
      output_vp8_info(false),
      output_clusters_size(false),
      output_encrypted_info(false) {
}

void Options::SetAll(bool value) {
  output_video = value;
  output_audio = value;
  output_size = value;
  output_offset = value;
  output_ebml_header = value;
  output_seconds = value;
  output_segment = value;
  output_segment_info = value;
  output_tracks = value;
  output_clusters = value;
  output_blocks = value;
  output_vp8_info = value;
  output_clusters_size = value;
  output_encrypted_info = value;
}

void Usage() {
  printf("Usage: webm_dump [options] -i input\n");
  printf("\n");
  printf("Main options:\n");
  printf("  -h | -?               show help\n");
  printf("  -v                    show version\n");
  printf("  -all <bool>           Set all output options to <bool>\n");
  printf("  -video <bool>         Output video tracks (true)\n");
  printf("  -audio <bool>         Output audio tracks (true)\n");
  printf("  -size <bool>          Output element sizes (false)\n");
  printf("  -offset <bool>        Output element offsets (false)\n");
  printf("  -times_seconds <bool> Output times as seconds (true)\n");
  printf("  -ebml_header <bool>   Output EBML header (true)\n");
  printf("  -segment <bool>       Output Segment (true)\n");
  printf("  -segment_info <bool>  Output SegmentInfo (true)\n");
  printf("  -tracks <bool>        Output Tracks (true)\n");
  printf("  -clusters <bool>      Output Clusters (false)\n");
  printf("  -blocks <bool>        Output Blocks (false)\n");
  printf("  -vp8_info <bool>      Output VP8 information (false)\n");
  printf("  -clusters_size <bool> Output Total Clusters size (false)\n");
  printf("  -encrypted_info <bool> Output encrypted frame info (false)\n");
}

// TODO(fgalligan): Add support for non-ascii.
wstring UTF8ToWideString(const char* str) {
  wstring wstr;

  if (str == NULL)
    return wstr;

  string temp_str(str, strlen(str));
  wstr.assign(temp_str.begin(), temp_str.end());

  return wstr;
}

void OutputEBMLHeader(const mkvparser::EBMLHeader& ebml,
                      FILE* o,
                      Indent* indent) {
  fprintf(o, "EBML Header:\n");
  indent->Adjust(webm_tools::kIncreaseIndent);
  fprintf(o, "%sEBMLVersion       : %lld\n",
          indent->indent_str().c_str(), ebml.m_version);
  fprintf(o, "%sEBMLReadVersion   : %lld\n",
          indent->indent_str().c_str(), ebml.m_readVersion);
  fprintf(o, "%sEBMLMaxIDLength   : %lld\n",
          indent->indent_str().c_str(), ebml.m_maxIdLength);
  fprintf(o, "%sEBMLMaxSizeLength : %lld\n",
          indent->indent_str().c_str(), ebml.m_maxSizeLength);
  fprintf(o, "%sDoc Type          : %s\n",
          indent->indent_str().c_str(), ebml.m_docType);
  fprintf(o, "%sDocTypeVersion    : %lld\n",
          indent->indent_str().c_str(), ebml.m_docTypeVersion);
  fprintf(o, "%sDocTypeReadVersion: %lld\n",
          indent->indent_str().c_str(), ebml.m_docTypeReadVersion);
  indent->Adjust(webm_tools::kDecreaseIndent);
}

void OutputSegment(const mkvparser::Segment& segment,
                   const Options& options,
                   FILE* o) {
  fprintf(o, "Segment:");
  if (options.output_offset)
    fprintf(o, "  @: %lld", segment.m_element_start);
  if (options.output_size)
    fprintf(o, "  size: %lld",
            segment.m_size + segment.m_start - segment.m_element_start);
  fprintf(o, "\n");
}

bool OutputSegmentInfo(const mkvparser::Segment& segment,
                       const Options& options,
                       FILE* o,
                       Indent* indent) {
  const mkvparser::SegmentInfo* const segment_info = segment.GetInfo();
  if (!segment_info) {
    fprintf(stderr, "SegmentInfo was NULL.\n");
    return false;
  }

  const int64 timecode_scale = segment_info->GetTimeCodeScale();
  const int64 duration_ns = segment_info->GetDuration();
  const wstring title = UTF8ToWideString(segment_info->GetTitleAsUTF8());
  const wstring muxing_app =
      UTF8ToWideString(segment_info->GetMuxingAppAsUTF8());
  const wstring writing_app =
      UTF8ToWideString(segment_info->GetWritingAppAsUTF8());

  fprintf(o, "%sSegmentInfo:", indent->indent_str().c_str());
  if (options.output_offset)
    fprintf(o, "  @: %lld", segment_info->m_element_start);
  if (options.output_size)
    fprintf(o, "  size: %lld", segment_info->m_element_size);
  fprintf(o, "\n");

  indent->Adjust(webm_tools::kIncreaseIndent);
  fprintf(o, "%sTimecodeScale : %lld \n",
          indent->indent_str().c_str(), timecode_scale);
  if (options.output_seconds)
    fprintf(o, "%sDuration(secs): %g\n",
            indent->indent_str().c_str(), duration_ns / kNanosecondsPerSecond);
  else
    fprintf(o, "%sDuration(nano): %lld\n",
            indent->indent_str().c_str(), duration_ns);

  if (!title.empty())
    fprintf(o, "%sTitle         : %ls\n",
            indent->indent_str().c_str(), title.c_str());
  if (!muxing_app.empty())
    fprintf(o, "%sMuxingApp     : %ls\n",
            indent->indent_str().c_str(), muxing_app.c_str());
  if (!writing_app.empty())
    fprintf(o, "%sWritingApp    : %ls\n",
            indent->indent_str().c_str(), writing_app.c_str());
  indent->Adjust(webm_tools::kDecreaseIndent);
  return true;
}

bool OutputTracks(const mkvparser::Segment& segment,
                  const Options& options,
                  FILE* o,
                  Indent* indent) {
  const mkvparser::Tracks* const tracks = segment.GetTracks();
  if (!tracks) {
    fprintf(stderr, "Tracks was NULL.\n");
    return false;
  }

  fprintf(o, "%sTracks:", indent->indent_str().c_str());
  if (options.output_offset)
    fprintf(o, "  @: %lld", tracks->m_element_start);
  if (options.output_size)
    fprintf(o, "  size: %lld", tracks->m_element_size);
  fprintf(o, "\n");

  unsigned int i = 0;
  const unsigned int j = tracks->GetTracksCount();
  while (i != j) {
    const mkvparser::Track* const track = tracks->GetTrackByIndex(i++);
    if (track == NULL)
      continue;

    indent->Adjust(webm_tools::kIncreaseIndent);
    fprintf(o, "%sTrack:", indent->indent_str().c_str());
    if (options.output_offset)
      fprintf(o, "  @: %lld", track->m_element_start);
    if (options.output_size)
      fprintf(o, "  size: %lld", track->m_element_size);
    fprintf(o, "\n");

    const int64 track_type = track->GetType();
    const int64 track_number = track->GetNumber();
    const wstring track_name = UTF8ToWideString(track->GetNameAsUTF8());

    indent->Adjust(webm_tools::kIncreaseIndent);
    fprintf(o, "%sTrackType   : %lld\n",
            indent->indent_str().c_str(), track_type);
    fprintf(o, "%sTrackNumber : %lld\n",
            indent->indent_str().c_str(), track_number);
    if (!track_name.empty())
      fprintf(o, "%sName        : %ls\n",
              indent->indent_str().c_str(), track_name.c_str());

    const char* const codec_id = track->GetCodecId();
    if (codec_id)
      fprintf(o, "%sCodecID     : %s\n",
              indent->indent_str().c_str(), codec_id);

    const wstring codec_name = UTF8ToWideString(track->GetCodecNameAsUTF8());
    if (!codec_name.empty())
      fprintf(o, "%sCodecName   : %ls\n",
              indent->indent_str().c_str(), codec_name.c_str());

    size_t private_size;
    const unsigned char* const private_data =
        track->GetCodecPrivate(private_size);
    if (private_data)
      fprintf(o, "%sPrivateData(size): %d\n",
              indent->indent_str().c_str(), static_cast<int>(private_size));

    if (track->GetContentEncodingCount() > 0) {
      // Only check the first content encoding.
      const ContentEncoding* const encoding =
          track->GetContentEncodingByIndex(0);
      if (!encoding) {
        printf("Could not get first ContentEncoding.\n");
        return NULL;
      }

      fprintf(o, "%sContentEncodingOrder : %lld\n",
              indent->indent_str().c_str(), encoding->encoding_order());
      fprintf(o, "%sContentEncodingScope : %lld\n",
              indent->indent_str().c_str(), encoding->encoding_scope());
      fprintf(o, "%sContentEncodingType  : %lld\n",
              indent->indent_str().c_str(), encoding->encoding_type());

      if (encoding->GetEncryptionCount() > 0) {
        // Only check the first encryption.
        const ContentEncoding::ContentEncryption* const encryption =
            encoding->GetEncryptionByIndex(0);
        if (!encryption) {
          printf("Could not get first ContentEncryption.\n");
          return false;
        }

        fprintf(o, "%sContentEncAlgo       : %lld\n",
                indent->indent_str().c_str(), encryption->algo);

        if (encryption->key_id_len > 0) {
          fprintf(o, "%sContentEncKeyID      : 0x",
                  indent->indent_str().c_str());
          for (int k = 0; k < encryption->key_id_len; ++k) {
            fprintf(o, "%x", encryption->key_id[k]);
          }
          fprintf(o, "\n");
        }

        if (encryption->signature_len > 0) {
          fprintf(o, "%sContentSignature     : 0x",
                  indent->indent_str().c_str());
          for (int k = 0; k < encryption->signature_len; ++k) {
            fprintf(o, "%x", encryption->signature[k]);
          }
          fprintf(o, "\n");
        }

        if (encryption->sig_key_id_len > 0) {
          fprintf(o, "%sContentSigKeyID      : 0x",
                  indent->indent_str().c_str());
          for (int k = 0; k < encryption->sig_key_id_len; ++k) {
            fprintf(o, "%x", encryption->sig_key_id[k]);
          }
          fprintf(o, "\n");
        }

        fprintf(o, "%sContentSigAlgo       : %lld\n",
                indent->indent_str().c_str(), encryption->sig_algo);
        fprintf(o, "%sContentSigHashAlgo   : %lld\n",
                indent->indent_str().c_str(), encryption->sig_hash_algo);

        const ContentEncoding::ContentEncAESSettings& aes =
            encryption->aes_settings;
        fprintf(o, "%sCipherMode           : %lld\n",
                indent->indent_str().c_str(), aes.cipher_mode);
      }
    }

    if (track_type == mkvparser::Track::kVideo) {
      const mkvparser::VideoTrack* const video_track =
          static_cast<const mkvparser::VideoTrack* const>(track);
      const int64 width = video_track->GetWidth();
      const int64 height = video_track->GetHeight();
      const double frame_rate = video_track->GetFrameRate();
      fprintf(o, "%sPixelWidth  : %lld\n",
              indent->indent_str().c_str(), width);
      fprintf(o, "%sPixelHeight : %lld\n",
              indent->indent_str().c_str(), height);
      if (frame_rate > 0.0)
        fprintf(o, "%sFrameRate   : %g\n",
                indent->indent_str().c_str(), video_track->GetFrameRate());
    } else if (track_type == mkvparser::Track::kAudio) {
      const mkvparser::AudioTrack* const audio_track =
          static_cast<const mkvparser::AudioTrack* const>(track);
      const int64 channels = audio_track->GetChannels();
      const int64 bit_depth = audio_track->GetBitDepth();
      fprintf(o, "%sChannels         : %lld\n",
              indent->indent_str().c_str(), channels);
      if (bit_depth > 0)
        fprintf(o, "%sBitDepth         : %lld\n",
                indent->indent_str().c_str(), bit_depth);
      fprintf(o, "%sSamplingFrequency: %g\n",
              indent->indent_str().c_str(), audio_track->GetSamplingRate());
    }
    indent->Adjust(webm_tools::kDecreaseIndent * 2);
  }

  return true;
}

bool OutputCluster(const mkvparser::Cluster& cluster,
                   const mkvparser::Tracks& tracks,
                   const Options& options,
                   FILE* o,
                   mkvparser::MkvReader* reader,
                   Indent* indent,
                   int64* clusters_size) {
  if (clusters_size) {
    // Load the Cluster.
    const mkvparser::BlockEntry* block_entry;
    int status = cluster.GetFirst(block_entry);
    if (status) {
      fprintf(stderr, "Could not get first Block of Cluster.\n");
      return false;
    }

    *clusters_size += cluster.GetElementSize();
  }

  if (options.output_clusters) {
    const int64 time_ns = cluster.GetTime();
    const int64 duration_ns = cluster.GetLastTime() - cluster.GetFirstTime();

    fprintf(o, "%sCluster:", indent->indent_str().c_str());
    if (options.output_offset)
      fprintf(o, "  @: %lld", cluster.m_element_start);
    if (options.output_size)
      fprintf(o, "  size: %lld", cluster.GetElementSize());
    fprintf(o, "\n");
    indent->Adjust(webm_tools::kIncreaseIndent);
    if (options.output_seconds)
      fprintf(o, "%sTimecode (sec) : %g\n",
              indent->indent_str().c_str(), time_ns / kNanosecondsPerSecond);
    else
      fprintf(o, "%sTimecode (nano): %lld\n",
              indent->indent_str().c_str(), time_ns);
    if (options.output_seconds)
      fprintf(o, "%sDuration (sec) : %g\n",
              indent->indent_str().c_str(),
              duration_ns / kNanosecondsPerSecond);
    else
      fprintf(o, "%sDuration (nano): %lld\n",
              indent->indent_str().c_str(), duration_ns);

    fprintf(o, "%s# Blocks       : %ld\n",
            indent->indent_str().c_str(), cluster.GetEntryCount());
  }

  if (options.output_blocks) {
    const mkvparser::BlockEntry* block_entry;
    int status = cluster.GetFirst(block_entry);
    if (status) {
      fprintf(stderr, "Could not get first Block of Cluster.\n");
      return false;
    }

    std::vector<unsigned char> vector_data;
    while (block_entry != NULL && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      if (!block) {
        fprintf(stderr, "Could not getblock entry.\n");
        return false;
      }

      const unsigned int track_number =
          static_cast<unsigned int>(block->GetTrackNumber());
      const mkvparser::Track* track = tracks.GetTrackByNumber(track_number);
      if (!track) {
        fprintf(stderr, "Could not get Track.\n");
        return false;
      }

      const int64 track_type = track->GetType();
      if ((track_type == mkvparser::Track::kVideo && options.output_video) ||
          (track_type == mkvparser::Track::kAudio && options.output_audio)) {
        const int64 time_ns = block->GetTime(&cluster);
        const bool is_key = block->IsKey();

        fprintf(o, "%sBlock: type:%s frame:%s",
                indent->indent_str().c_str(),
                track_type == mkvparser::Track::kVideo ? "V" : "A",
                is_key ? "I" : "P");
        if (options.output_seconds)
          fprintf(o, " secs:%5g", time_ns / kNanosecondsPerSecond);
        else
          fprintf(o, " nano:%10lld", time_ns);

        if (options.output_offset)
          fprintf(o, " @_payload: %lld", block->m_start);
        if (options.output_size)
          fprintf(o, " size_payload: %lld", block->m_size);

        const int kChecksumSize = 12;
        const uint8 KEncryptedBit = 0x1;
        const int kSignalByteSize = 1;
        bool encrypted_stream = false;
        if (options.output_encrypted_info) {
          if (track->GetContentEncodingCount() > 0) {
            // Only check the first content encoding.
            const ContentEncoding* const encoding =
                track->GetContentEncodingByIndex(0);
            if (encoding) {
              if (encoding->GetEncryptionCount() > 0) {
                const ContentEncoding::ContentEncryption* const encryption =
                    encoding->GetEncryptionByIndex(0);
                if (encryption) {
                  const ContentEncoding::ContentEncAESSettings& aes =
                      encryption->aes_settings;
                  if (aes.cipher_mode == 1) {
                    encrypted_stream = true;
                  }
                }
              }
            }
          }

          if (encrypted_stream) {
            const mkvparser::Block::Frame& frame = block->GetFrame(0);
            if (frame.len > static_cast<int>(vector_data.size())) {
              vector_data.resize(frame.len + 1024);
            }

            unsigned char* data = &vector_data[0];
            if (frame.Read(reader, data) < 0) {
              fprintf(stderr, "Could not read frame.\n");
              return false;
            }

            const bool encrypted_frame =
                (data[kChecksumSize] & KEncryptedBit) ? 1 : 0;
            fprintf(o, " enc: %d", encrypted_frame ? 1 : 0);

            if (encrypted_frame) {
              uint64 iv;
              memcpy(&iv, data + (kChecksumSize + kSignalByteSize), sizeof(iv));
              iv = webm_tools::bigendian_to_host(iv);
              fprintf(o, " iv: %lld", iv);
            }
          }
        }

        if (options.output_vp8_info) {
          const int frame_count = block->GetFrameCount();

          if (frame_count > 1) {
            fprintf(o, "\n");
            indent->Adjust(webm_tools::kIncreaseIndent);
          }

          for (int i = 0; i < frame_count; ++i) {
            if (track_type == mkvparser::Track::kVideo) {
              const mkvparser::Block::Frame& frame = block->GetFrame(i);
              if (frame.len > static_cast<int>(vector_data.size())) {
                vector_data.resize(frame.len + 1024);
              }

              unsigned char* data = &vector_data[0];
              if (frame.Read(reader, data) < 0) {
                fprintf(stderr, "Could not read frame.\n");
                return false;
              }

              if (frame_count > 1)
                fprintf(o, "\n%sVP8 data     :", indent->indent_str().c_str());

              bool encrypted_frame = false;
              int frame_offset = 0;
              if (encrypted_stream) {
                if (data[kChecksumSize] & KEncryptedBit) {
                  encrypted_frame = true;
                } else {
                  frame_offset = kChecksumSize + kSignalByteSize;
                }
              }

              if (!encrypted_frame) {
                data += frame_offset;
                const unsigned int temp =
                  (data[2] << 16) | (data[1] << 8) | data[0];

                const int vp8_key = !(temp & 0x1);
                const int vp8_version = (temp >> 1) & 0x7;
                const int vp8_altref = !((temp >> 4) & 0x1);
                const int vp8_partition_length = (temp >> 5) & 0x7FFFF;
                fprintf(o, " key:%d v:%d altref:%d partition_length:%d", 
                        vp8_key, vp8_version, vp8_altref, vp8_partition_length);
              }
            }
          }

          if (frame_count > 1)
            indent->Adjust(webm_tools::kDecreaseIndent);
        }

        fprintf(o, "\n");
      }

      status = cluster.GetNext(block_entry, block_entry);
      if (status) {
        printf("\n Could not get next block of cluster.\n");
        return false;
      }
    }
  }

  if (options.output_clusters)
    indent->Adjust(webm_tools::kDecreaseIndent);

  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  string input;
  Options options;

  const int argc_check = argc - 1;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp("-h", argv[i]) || !strcmp("-?", argv[i])) {
      Usage();
      return EXIT_SUCCESS;
    } else if (!strcmp("-v", argv[i])) {
      printf("version: %s\n", VERSION_STRING);
    } else if (!strcmp("-i", argv[i]) && i < argc_check) {
      input = argv[++i];
    } else if (!strcmp("-all", argv[i]) && i < argc_check) {
      const bool value = !!strcmp("false", argv[++i]);
      options.SetAll(value);
    } else if (!strcmp("-video", argv[i]) && i < argc_check) {
      options.output_video = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-audio", argv[i]) && i < argc_check) {
      options.output_audio = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-size", argv[i]) && i < argc_check) {
      options.output_size = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-offset", argv[i]) && i < argc_check) {
      options.output_offset = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-times_seconds", argv[i]) && i < argc_check) {
      options.output_seconds = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-ebml_header", argv[i]) && i < argc_check) {
      options.output_ebml_header = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-segment", argv[i]) && i < argc_check) {
      options.output_segment = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-segment_info", argv[i]) && i < argc_check) {
      options.output_segment_info = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-tracks", argv[i]) && i < argc_check) {
      options.output_tracks = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-clusters", argv[i]) && i < argc_check) {
      options.output_clusters = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-blocks", argv[i]) && i < argc_check) {
      options.output_blocks = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-vp8_info", argv[i]) && i < argc_check) {
      options.output_vp8_info = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-clusters_size", argv[i]) && i < argc_check) {
      options.output_clusters_size = !!strcmp("false", argv[++i]);
    } else if (!strcmp("-encrypted_info", argv[i]) && i < argc_check) {
      options.output_encrypted_info = !!strcmp("false", argv[++i]);
    }
  }

  if (argc < 3 || input.empty()) {
    Usage();
    return EXIT_FAILURE;
  }

  // TODO(fgalligan): Replace auto_ptr with scoped_ptr.
  std::auto_ptr<mkvparser::MkvReader>
      reader(new (std::nothrow) mkvparser::MkvReader());  // NOLINT
  if (reader->Open(input.c_str())) {
    fprintf(stderr, "Error opening file:%s\n", input.c_str());
    return EXIT_FAILURE;
  }

  int64 pos = 0;
  std::auto_ptr<mkvparser::EBMLHeader>
      ebml_header(new (std::nothrow) mkvparser::EBMLHeader());  // NOLINT
  if (ebml_header->Parse(reader.get(), pos) < 0) {
    fprintf(stderr, "Error parsing EBML header.\n");
    return EXIT_FAILURE;
  }

  Indent indent(0);
  FILE* out = stdout;

  if (options.output_ebml_header)
    OutputEBMLHeader(*ebml_header.get(), out, &indent);

  mkvparser::Segment* temp_segment;
  if (mkvparser::Segment::CreateInstance(reader.get(), pos, temp_segment)) {
    fprintf(stderr, "Segment::CreateInstance() failed.\n");
    return EXIT_FAILURE;
  }
  std::auto_ptr<mkvparser::Segment> segment(temp_segment);

  if (segment->Load() < 0) {
      fprintf(stderr, "Segment::Load() failed.\n");
      return EXIT_FAILURE;
  }

  if (options.output_segment) {
    OutputSegment(*(segment.get()), options, out);
    indent.Adjust(webm_tools::kIncreaseIndent);
  }

  if (options.output_segment_info)
    if (!OutputSegmentInfo(*(segment.get()), options, out, &indent))
      return EXIT_FAILURE;

  if (options.output_tracks)
    if (!OutputTracks(*(segment.get()), options, out, &indent))
      return EXIT_FAILURE;

  if (options.output_clusters)
    fprintf(out, "%sClusters (count):%ld\n",
            indent.indent_str().c_str(), segment->GetCount());

  const mkvparser::Tracks* const tracks = segment->GetTracks();
  if (!tracks) {
    fprintf(stderr, "Could not get Tracks.\n");
    return EXIT_FAILURE;
  }

  int64 clusters_size = 0;
  const mkvparser::Cluster* cluster = segment->GetFirst();
  while (cluster != NULL && !cluster->EOS()) {
    if (!OutputCluster(*cluster,
                       *tracks,
                       options,
                       out,
                       reader.get(),
                       &indent,
                       &clusters_size))
      return EXIT_FAILURE;
    cluster = segment->GetNext(cluster);
  }

  if (options.output_clusters_size)
    fprintf(out, "%sClusters (size):%lld\n",
            indent.indent_str().c_str(), clusters_size);

  return EXIT_SUCCESS;
}
