// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include <cstring>
#include <limits>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "indent.h"
#include "mkvreader.hpp"
#include "mkvparser.hpp"
#include "webm_constants.h"
#include "webm_endian.h"

namespace {

using mkvparser::ContentEncoding;
using std::string;
using std::wstring;
using webm_tools::Indent;
using webm_tools::int64;
using webm_tools::uint8;
using webm_tools::uint32;
using webm_tools::uint64;
using webm_tools::kNanosecondsPerSecond;
using webm_tools::kNanosecondsPerSecondi;

const char VERSION_STRING[] = "1.0.3.0";

struct Options {
  Options();

  // Returns true if |value| matches -|option| or -no|option|.
  static bool MatchesBooleanOption(const string& option, const string& value);

  // Set all of the member variables to |value|.
  void SetAll(bool value);

  bool output_video;
  bool output_audio;
  bool output_size;
  bool output_offset;
  bool output_seconds;
  bool output_ebml_header;
  bool output_segment;
  bool output_seekhead;
  bool output_segment_info;
  bool output_tracks;
  bool output_clusters;
  bool output_blocks;
  bool output_codec_info;
  bool output_clusters_size;
  bool output_encrypted_info;
  bool output_cues;
  bool output_frame_stats;
};

Options::Options()
    : output_video(true),
      output_audio(true),
      output_size(false),
      output_offset(false),
      output_seconds(true),
      output_ebml_header(true),
      output_segment(true),
      output_seekhead(false),
      output_segment_info(true),
      output_tracks(true),
      output_clusters(false),
      output_blocks(false),
      output_codec_info(false),
      output_clusters_size(false),
      output_encrypted_info(false),
      output_cues(false),
      output_frame_stats(false) {
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
  output_codec_info = value;
  output_clusters_size = value;
  output_encrypted_info = value;
  output_cues = value;
  output_frame_stats = value;
}

bool Options::MatchesBooleanOption(const string& option, const string& value) {
  const string opt = "-" + option;
  const string noopt = "-no" + option;
  return value == opt || value == noopt;
}

struct FrameStats {
  FrameStats()
    : frames(0),
      displayed_frames(0),
      first_altref(true),
      frames_since_last_altref(0),
      minimum_altref_distance(std::numeric_limits<int>::max()),
      min_altref_end_ns(0),
      max_window_size(0),
      max_window_end_ns(0) {
  }

  int frames;
  int displayed_frames;

  bool first_altref;
  int frames_since_last_altref;
  int minimum_altref_distance;
  int64 min_altref_end_ns;

  std::queue<int64> window;
  int64 max_window_size;
  int64 max_window_end_ns;
};

void Usage() {
  printf("Usage: webm_info [options] -i input\n");
  printf("\n");
  printf("Main options:\n");
  printf("  -h | -?               show help\n");
  printf("  -v                    show version\n");
  printf("  -all                  Enable all output options.\n");
  printf("  -video                Output video tracks (true)\n");
  printf("  -audio                Output audio tracks (true)\n");
  printf("  -size                 Output element sizes (false)\n");
  printf("  -offset               Output element offsets (false)\n");
  printf("  -times_seconds        Output times as seconds (true)\n");
  printf("  -ebml_header          Output EBML header (true)\n");
  printf("  -segment              Output Segment (true)\n");
  printf("  -seekhead             Output SeekHead (false)\n");
  printf("  -segment_info         Output SegmentInfo (true)\n");
  printf("  -tracks               Output Tracks (true)\n");
  printf("  -clusters             Output Clusters (false)\n");
  printf("  -blocks               Output Blocks (false)\n");
  printf("  -codec_info           Output video codec information (false)\n");
  printf("  -clusters_size        Output Total Clusters size (false)\n");
  printf("  -encrypted_info       Output encrypted frame info (false)\n");
  printf("  -cues                 Output Cues entries (false)\n");
  printf("  -frame_stats          Output frame stats (VP9)(false)\n");
  printf("\nOutput options may be negated by prefixing 'no'.\n");
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

bool OutputSeekHead(const mkvparser::Segment& segment,
                    const Options& options, FILE* o, Indent* indent) {
  const mkvparser::SeekHead* const seekhead = segment.GetSeekHead();
  if (!seekhead) {
    // SeekHeads are optional.
    return true;
  }

  fprintf(o, "%sSeekHead:", indent->indent_str().c_str());
  if (options.output_offset)
    fprintf(o, "  @: %lld", seekhead->m_element_start);
  if (options.output_size)
    fprintf(o, "  size: %lld", seekhead->m_element_size);
  fprintf(o, "\n");

  indent->Adjust(webm_tools::kIncreaseIndent);

  for (int i = 0; i < seekhead->GetCount(); ++i) {
    const mkvparser::SeekHead::Entry* const entry = seekhead->GetEntry(i);
    if (!entry) {
      fprintf(stderr, "Error retrieving SeekHead entry #%d\n", i);
      return false;
    }

    fprintf(o, "%sEntry[%d]", indent->indent_str().c_str(), i);
    if (options.output_offset)
      fprintf(o, "  @: %lld", entry->element_start);
    if (options.output_size)
      fprintf(o, "  size: %lld", entry->element_size);
    fprintf(o, "\n");

    indent->Adjust(webm_tools::kIncreaseIndent);
    const char* const entry_indent = indent->indent_str().c_str();
    // TODO(jzern): 1) known ids could be stringified. 2) ids could be
    // reencoded to EBML for ease of lookup.
    fprintf(o, "%sSeek ID       : %llx\n", entry_indent, entry->id);
    fprintf(o, "%sSeek position : %lld\n", entry_indent, entry->pos);
    indent->Adjust(webm_tools::kDecreaseIndent);
  }

  for (int i = 0; i < seekhead->GetVoidElementCount(); ++i) {
    const mkvparser::SeekHead::VoidElement* const entry =
        seekhead->GetVoidElement(i);
    if (!entry) {
      fprintf(stderr, "Error retrieving SeekHead void element #%d\n", i);
      return false;
    }

    fprintf(o, "%sVoid element[%d]", indent->indent_str().c_str(), i);
    if (options.output_offset)
      fprintf(o, "  @: %lld", entry->element_start);
    if (options.output_size)
      fprintf(o, "  size: %lld", entry->element_size);
    fprintf(o, "\n");
  }

  indent->Adjust(webm_tools::kDecreaseIndent);
  return true;
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

    const uint64 default_duration = track->GetDefaultDuration();
    if (default_duration > 0)
      fprintf(o, "%sDefaultDuration: %llu\n",
              indent->indent_str().c_str(), default_duration);

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
          fprintf(o, "%sContentEncKeyID      : ",
                  indent->indent_str().c_str());
          for (int k = 0; k < encryption->key_id_len; ++k) {
            fprintf(o, "0x%02x, ", encryption->key_id[k]);
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
      const int64 display_width = video_track->GetDisplayWidth();
      const int64 display_height = video_track->GetDisplayHeight();
      const int64 display_unit = video_track->GetDisplayUnit();
      const double frame_rate = video_track->GetFrameRate();
      fprintf(o, "%sPixelWidth  : %lld\n",
              indent->indent_str().c_str(), width);
      fprintf(o, "%sPixelHeight : %lld\n",
              indent->indent_str().c_str(), height);
      if (frame_rate > 0.0)
        fprintf(o, "%sFrameRate   : %g\n",
                indent->indent_str().c_str(), video_track->GetFrameRate());
      if (display_unit > 0 || display_width != width ||
          display_height != height) {
        fprintf(o, "%sDisplayWidth  : %lld\n", indent->indent_str().c_str(),
                display_width);
        fprintf(o, "%sDisplayHeight : %lld\n", indent->indent_str().c_str(),
                display_height);
        fprintf(o, "%sDisplayUnit   : %lld\n", indent->indent_str().c_str(),
                display_unit);
      }
    } else if (track_type == mkvparser::Track::kAudio) {
      const mkvparser::AudioTrack* const audio_track =
          static_cast<const mkvparser::AudioTrack* const>(track);
      const int64 channels = audio_track->GetChannels();
      const int64 bit_depth = audio_track->GetBitDepth();
      const uint64 codec_delay = audio_track->GetCodecDelay();
      const uint64 seek_preroll = audio_track->GetSeekPreRoll();
      fprintf(o, "%sChannels         : %lld\n",
              indent->indent_str().c_str(), channels);
      if (bit_depth > 0)
        fprintf(o, "%sBitDepth         : %lld\n",
                indent->indent_str().c_str(), bit_depth);
      fprintf(o, "%sSamplingFrequency: %g\n",
              indent->indent_str().c_str(), audio_track->GetSamplingRate());
      if (codec_delay)
        fprintf(o, "%sCodecDelay       : %llu\n",
                indent->indent_str().c_str(), codec_delay);
      if (seek_preroll)
        fprintf(o, "%sSeekPreRoll      : %llu\n",
                indent->indent_str().c_str(), seek_preroll);
    }
    indent->Adjust(webm_tools::kDecreaseIndent * 2);
  }

  return true;
}

// libvpx reference: vp9/vp9_dx_iface.c
void ParseSuperframeIndex(const uint8* data, size_t data_sz,
                          uint32 sizes[8], int* count) {
  const uint8 marker = data[data_sz - 1];
  *count = 0;

  if ((marker & 0xe0) == 0xc0) {
    const int frames = (marker & 0x7) + 1;
    const int mag = ((marker >> 3) & 0x3) + 1;
    const size_t index_sz = 2 + mag * frames;

    if (data_sz >= index_sz && data[data_sz - index_sz] == marker) {
      // found a valid superframe index
      const uint8* x = data + data_sz - index_sz + 1;

      for (int i = 0; i < frames; ++i) {
        uint32 this_sz = 0;

        for (int j = 0; j < mag; ++j) {
          this_sz |= (*x++) << (j * 8);
        }
        sizes[i] = this_sz;
      }
      *count = frames;
    }
  }
}

void PrintVP9Info(const uint8* data, int size, FILE* o, int64 time_ns,
                  FrameStats* stats) {
  if (size < 1) return;

  uint32 sizes[8];
  int i = 0, count = 0;
  ParseSuperframeIndex(data, size, sizes, &count);

  // Remove all frames that are less than window size.
  while (!stats->window.empty() &&
         stats->window.front() < (time_ns - (kNanosecondsPerSecondi - 1)))
    stats->window.pop();

  do {
    // const int frame_marker = (data[0] >> 6) & 0x3;
    // TODO(jzern): profile > 2 uses 3 bits in the header.
    const int version = (data[0] >> 4) & 0x3;
    const int key = !((data[0] >> 2) & 0x1);
    const int altref_frame = !((data[0] >> 1) & 0x1);
    const int error_resilient_mode = data[0] & 0x1;
    if (version > 2) {
      fprintf(o, " profile > 2 is unsupported");
      return;
    }

    if (key &&
        !(size >= 4 && data[1] == 0x49 && data[2] == 0x83 && data[3] == 0x42)) {
      fprintf(o, " invalid VP9 signature");
      return;
    }

    stats->window.push(time_ns);
    ++stats->frames;

    if (altref_frame) {
      const int delta_altref = stats->frames_since_last_altref;
      if (stats->first_altref) {
        stats->first_altref = false;
      } else if (delta_altref < stats->minimum_altref_distance) {
        stats->minimum_altref_distance = delta_altref;
        stats->min_altref_end_ns = time_ns;
      }
      stats->frames_since_last_altref = 0;
    } else {
      ++stats->frames_since_last_altref;
      ++stats->displayed_frames;
    }

    if (count > 0) {
      fprintf(o, " packed [%d]: {", i);
    }

    fprintf(o, " key:%d v:%d altref:%d errm:%d",
            key, version, altref_frame, error_resilient_mode);

    if (key && size > 4) {
      fprintf(o, " cs:%d", (data[4] >> 5) & 0x7);
    }

    if (count > 0) {
      fprintf(o, " size: %u }", sizes[i]);
      data += sizes[i];
      size -= sizes[i];
    }
    ++i;
  } while (i < count);

  if (stats->max_window_size < static_cast<int64>(stats->window.size())) {
    stats->max_window_size = stats->window.size();
    stats->max_window_end_ns = time_ns;
  }
}

void PrintVP8Info(const uint8* data, int size, FILE* o) {
  if (size < 3) return;

  const uint32 bits = data[0] | (data[1] << 8) | (data[2] << 16);
  const int key = !(bits & 0x1);
  const int altref_frame = !((bits >> 4) & 0x1);
  const int version = (bits >> 1) & 0x7;
  const int partition_length = (bits >> 5) & 0x7FFFF;
  if (key &&
      !(size >= 6 && data[3] == 0x9d && data[4] == 0x01 && data[5] == 0x2a)) {
    fprintf(o, " invalid VP8 signature");
    return;
  }
  fprintf(o, " key:%d v:%d altref:%d partition_length:%d",
          key, version, altref_frame, partition_length);
}

bool OutputCluster(const mkvparser::Cluster& cluster,
                   const mkvparser::Tracks& tracks,
                   const Options& options,
                   FILE* o,
                   mkvparser::MkvReader* reader,
                   Indent* indent,
                   int64* clusters_size,
                   FrameStats* stats) {
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

        if (block_entry->GetKind() == mkvparser::BlockEntry::kBlockGroup) {
          fprintf(o, "%sBlockGroup:\n", indent->indent_str().c_str());
          indent->Adjust(webm_tools::kIncreaseIndent);
        }

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

            const bool encrypted_frame = (data[0] & KEncryptedBit) ? 1 : 0;
            fprintf(o, " enc: %d", encrypted_frame ? 1 : 0);

            if (encrypted_frame) {
              uint64 iv;
              memcpy(&iv, data + kSignalByteSize, sizeof(iv));
              fprintf(o, " iv: %llx", iv);
            }
          }
        }

        if (options.output_codec_info) {
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
                if (data[0] & KEncryptedBit) {
                  encrypted_frame = true;
                } else {
                  frame_offset = kSignalByteSize;
                }
              }

              if (!encrypted_frame) {
                data += frame_offset;

                const string codec_id = track->GetCodecId();
                if (codec_id == "V_VP8") {
                  PrintVP8Info(data, frame.len, o);
                } else if (codec_id == "V_VP9") {
                  PrintVP9Info(data, frame.len, o, time_ns, stats);
                }
              }
            }
          }

          if (frame_count > 1)
            indent->Adjust(webm_tools::kDecreaseIndent);
        }

        if (block_entry->GetKind() == mkvparser::BlockEntry::kBlockGroup) {
          const int64 discard_padding = block->GetDiscardPadding();
          if (discard_padding != 0) {
            fprintf(o, "\n%sDiscardPadding: %10lld",
                    indent->indent_str().c_str(), discard_padding);
          }
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

bool OutputCues(const mkvparser::Segment& segment,
                const mkvparser::Tracks& tracks,
                const Options& options,
                FILE* o,
                Indent* indent) {
  const mkvparser::Cues* const cues = segment.GetCues();
  if (cues == NULL)
    return true;

  // Load all of the cue points.
  while (!cues->DoneParsing())
    cues->LoadCuePoint();

  // Confirm that the input has cue points.
  const mkvparser::CuePoint* const first_cue = cues->GetFirst();
  if (first_cue == NULL) {
    fprintf(o, "%sNo cue points.\n", indent->indent_str().c_str());
    return true;
  }

  // Input has cue points, dump them:
  fprintf(o, "%sCues:", indent->indent_str().c_str());
  if (options.output_offset)
    fprintf(o, " @:%lld", cues->m_element_start);
  if (options.output_size)
    fprintf(o, " size:%lld", cues->m_element_size);
  fprintf(o, "\n");

  const mkvparser::CuePoint* cue_point = first_cue;
  int cue_point_num = 1;
  const int num_tracks = tracks.GetTracksCount();
  indent->Adjust(webm_tools::kIncreaseIndent);

  do {
    for (int track_num = 1; track_num <= num_tracks; ++track_num) {
      const mkvparser::Track* const track = tracks.GetTrackByNumber(track_num);
      const mkvparser::CuePoint::TrackPosition* const track_pos =
          cue_point->Find(track);

      if (track_pos != NULL) {
        const char track_type =
            (track->GetType() == mkvparser::Track::kVideo) ? 'V' : 'A';
        fprintf(o, "%sCue Point:%d type:%c track:%d",
                indent->indent_str().c_str(), cue_point_num,
                track_type, track_num);

        if (options.output_seconds) {
          fprintf(o, " secs:%g",
                  cue_point->GetTime(&segment) / kNanosecondsPerSecond);
        } else {
          fprintf(o, " nano:%lld", cue_point->GetTime(&segment));
        }

        if (options.output_blocks)
          fprintf(o, " block:%lld", track_pos->m_block);

        if (options.output_offset)
          fprintf(o, " @:%lld", track_pos->m_pos);

        fprintf(o, "\n");
      }
    }

    cue_point = cues->GetNext(cue_point);
    ++cue_point_num;
  } while (cue_point != NULL);

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
    } else if (!strcmp("-all", argv[i])) {
      options.SetAll(true);
    } else if (Options::MatchesBooleanOption("video", argv[i])) {
      options.output_video = !strcmp("-video", argv[i]);
    } else if (Options::MatchesBooleanOption("audio", argv[i])) {
      options.output_audio = !strcmp("-audio", argv[i]);
    } else if (Options::MatchesBooleanOption("size", argv[i])) {
      options.output_size = !strcmp("-size", argv[i]);
    } else if (Options::MatchesBooleanOption("offset", argv[i])) {
      options.output_offset = !strcmp("-offset", argv[i]);
    } else if (Options::MatchesBooleanOption("times_seconds", argv[i])) {
      options.output_seconds = !strcmp("-times_seconds", argv[i]);
    } else if (Options::MatchesBooleanOption("ebml_header", argv[i])) {
      options.output_ebml_header = !strcmp("-ebml_header", argv[i]);
    } else if (Options::MatchesBooleanOption("segment", argv[i])) {
      options.output_segment = !strcmp("-segment", argv[i]);
    } else if (Options::MatchesBooleanOption("seekhead", argv[i])) {
      options.output_seekhead = !strcmp("-seekhead", argv[i]);
    } else if (Options::MatchesBooleanOption("segment_info", argv[i])) {
      options.output_segment_info = !strcmp("-segment_info", argv[i]);
    } else if (Options::MatchesBooleanOption("tracks", argv[i])) {
      options.output_tracks = !strcmp("-tracks", argv[i]);
    } else if (Options::MatchesBooleanOption("clusters", argv[i])) {
      options.output_clusters = !strcmp("-clusters", argv[i]);
    } else if (Options::MatchesBooleanOption("blocks", argv[i])) {
      options.output_blocks = !strcmp("-blocks", argv[i]);
    } else if (Options::MatchesBooleanOption("codec_info", argv[i])) {
      options.output_codec_info = !strcmp("-codec_info", argv[i]);
    } else if (Options::MatchesBooleanOption("clusters_size", argv[i])) {
      options.output_clusters_size = !strcmp("-clusters_size", argv[i]);
    } else if (Options::MatchesBooleanOption("encrypted_info", argv[i])) {
      options.output_encrypted_info = !strcmp("-encrypted_info", argv[i]);
    } else if (Options::MatchesBooleanOption("cues", argv[i])) {
      options.output_cues = !strcmp("-cues", argv[i]);
    } else if (Options::MatchesBooleanOption("frame_stats", argv[i])) {
      options.output_frame_stats = !strcmp("-frame_stats", argv[i]);
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

  if (options.output_seekhead)
    if (!OutputSeekHead(*(segment.get()), options, out, &indent))
      return EXIT_FAILURE;

  if (options.output_segment_info)
    if (!OutputSegmentInfo(*(segment.get()), options, out, &indent))
      return EXIT_FAILURE;

  if (options.output_tracks)
    if (!OutputTracks(*(segment.get()), options, out, &indent))
      return EXIT_FAILURE;

  const mkvparser::Tracks* const tracks = segment->GetTracks();
  if (!tracks) {
    fprintf(stderr, "Could not get Tracks.\n");
    return EXIT_FAILURE;
  }

  // If Cues are before the clusters output them first.
  if (options.output_cues) {
    const mkvparser::Cluster* cluster = segment->GetFirst();
    const mkvparser::Cues* const cues = segment->GetCues();
    if (cluster != NULL && cues != NULL) {
      if (cues->m_element_start < cluster->m_element_start) {
        if (!OutputCues(*segment, *tracks, options, out, &indent)) {
          return EXIT_FAILURE;
        }
        options.output_cues = false;
      }
    }
  }

  if (options.output_clusters)
    fprintf(out, "%sClusters (count):%ld\n",
            indent.indent_str().c_str(), segment->GetCount());

  int64 clusters_size = 0;
  FrameStats stats;
  const mkvparser::Cluster* cluster = segment->GetFirst();
  while (cluster != NULL && !cluster->EOS()) {
    if (!OutputCluster(*cluster,
                       *tracks,
                       options,
                       out,
                       reader.get(),
                       &indent,
                       &clusters_size,
                       &stats))
      return EXIT_FAILURE;
    cluster = segment->GetNext(cluster);
  }

  if (options.output_clusters_size)
    fprintf(out, "%sClusters (size):%lld\n",
            indent.indent_str().c_str(), clusters_size);

  if (options.output_cues)
    if (!OutputCues(*segment, *tracks, options, out, &indent))
      return EXIT_FAILURE;

  // TODO(fgalligan): Add support for VP8.
  if (options.output_frame_stats &&
      stats.minimum_altref_distance != std::numeric_limits<int>::max()) {
    const double actual_fps =
        stats.frames / (segment->GetInfo()->GetDuration() /
                        kNanosecondsPerSecond);
    const double displayed_fps =
        stats.displayed_frames / (segment->GetInfo()->GetDuration() /
                                  kNanosecondsPerSecond);
    fprintf(out, "\nActual fps:%g  Displayed fps:%g\n",
            actual_fps, displayed_fps);

    fprintf(out, "Minimum Altref Distance:%d  at:%g seconds\n",
            stats.minimum_altref_distance,
            stats.min_altref_end_ns / kNanosecondsPerSecond);

    // TODO(fgalligan): Add support for window duration other than 1 second.
    const double sec_end = stats.max_window_end_ns / kNanosecondsPerSecond;
    const double sec_start =
        stats.max_window_end_ns > kNanosecondsPerSecondi ? sec_end - 1.0 : 0.0;
    fprintf(out, "Maximum Window:%g-%g seconds  Window fps:%lld\n",
            sec_start, sec_end, stats.max_window_size);
  }
  return EXIT_SUCCESS;
}
