// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <iostream>
#include <string>

#include "base/base_switches.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "mkvmuxer.hpp"
#include "mkvmuxerutil.hpp"
#include "mkvparser.hpp"
#include "mkvreader.hpp"
#include "mkvwriter.hpp"

using std::string;
using std::vector;

// This application uses the crypto library from the Chromium project. See the
// readme.txt for build instructions.

namespace {

void Usage() {
  printf("Usage: webm_crypt [-test] -i <input> -o <output> [options]\n");
  printf("\n");
  printf("Main options:\n");
  printf("  -h | -?              Show help.\n");
  printf("  -test                Tests the encryption and decryption.\n");
  printf("  -key_file <string>   Path to key file. (Default empty)\n");
  printf("  -content_id <string> Encryption content ID. (Default empty)\n");
  printf("  -audio <bool>        Act on audio stream. (Default false)\n");
  printf("  -video <bool>        Act on video stream. (Default true)\n");
  printf("  -encrypt <bool>      True/False encrypt/decrypt (Default true)\n");
}

void TestEncryption() {
  scoped_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::GenerateRandomKey(
          crypto::SymmetricKey::AES, 128));

  const std::string kInitialCounter = "0000000000000000";

  std::string raw_key;
  if (!key->GetRawKey(&raw_key)) {
    std::cout << "Could not get raw key." << std::endl;
    return;
  }

  crypto::Encryptor encryptor;
  // The IV must be exactly as long as the cipher block size.
  bool b = encryptor.Init(key.get(), crypto::Encryptor::CBC, kInitialCounter);
  if (!b) {
    std::cout << "Could not initialize encrypt object." << std::endl;
    return;
  }

  std::string plaintext("this is the plaintext");
  std::string ciphertext;
  b = encryptor.Encrypt(plaintext, &ciphertext);
  if (!b) {
    std::cout << "Could not encrypt data." << std::endl;
    return;
  }

  std::string decrypted;
  b = encryptor.Decrypt(ciphertext, &decrypted);
  if (!b) {
    std::cout << "Could not decrypt data." << std::endl;
    return;
  }

  std::cout << "Test 1 finished." << std::endl;
  std::cout << "iv         :" << kInitialCounter << std::endl;
  std::cout << "raw_key    :" << raw_key << std::endl;
  std::cout << "plaintext  :" << plaintext << std::endl;
  std::cout << "ciphertext :" << ciphertext << std::endl;
  std::cout << "decrypted   :" << decrypted << std::endl;

  const int kNonAsciiSize = 256;
  std::string non_ascii;
  non_ascii.resize(kNonAsciiSize);
  char* raw_message_buf = const_cast<char*>(non_ascii.data());
  for (int i = 0; i < kNonAsciiSize; ++i) {
    raw_message_buf[i] = i;
  }

  b = encryptor.Encrypt(non_ascii, &ciphertext);
  if (!b) {
    std::cout << "Could not encrypt data." << std::endl;
    return;
  }

  b = encryptor.Decrypt(ciphertext, &decrypted);
  if (!b) {
    std::cout << "Could not decrypt data." << std::endl;
    return;
  }

  std::cout << "Test 2 finished." << std::endl;
  std::cout << "iv         :" << kInitialCounter << std::endl;
  std::cout << "non_ascii  :" << non_ascii << std::endl;
  std::cout << "ciphertext :" << ciphertext << std::endl;
  std::cout << "decrypted   :" << decrypted << std::endl;


  const int kNonAsciiSizePlus = kNonAsciiSize + plaintext.size();
  non_ascii.resize(kNonAsciiSizePlus);
  raw_message_buf = const_cast<char*>(non_ascii.data());
  for (int i = 0; i < kNonAsciiSize; ++i) {
    raw_message_buf[i] = i;
  }
  memcpy(raw_message_buf + kNonAsciiSize, plaintext.data(), plaintext.size());

  b = encryptor.Encrypt(non_ascii, &ciphertext);
  if (!b) {
    std::cout << "Could not encrypt data." << std::endl;
    return;
  }

  b = encryptor.Decrypt(ciphertext, &decrypted);
  if (!b) {
    std::cout << "Could not decrypt data." << std::endl;
    return;
  }

  std::cout << "Test 3 finished." << std::endl;
  std::cout << "iv         :" << kInitialCounter << std::endl;
  std::cout << "non_ascii  :" << non_ascii << std::endl;
  std::cout << "ciphertext :" << ciphertext << std::endl;
  std::cout << "decrypted   :" << decrypted << std::endl;
  std::cout << "Tests passed." << std::endl;
}


bool OpenWebMFiles(mkvparser::MkvReader& reader,
                   const std::string& input,
                   scoped_ptr<mkvparser::Segment>& parser_segment,
                   mkvmuxer::MkvWriter& writer,
                   const std::string& output,
                   scoped_ptr<mkvmuxer::Segment>& muxer_segment) {
  if (reader.Open(input.c_str())) {
    printf("\n Filename is invalid or error while opening.\n");
    return false;
  }

  long long pos = 0;
  mkvparser::EBMLHeader ebml_header;
  ebml_header.Parse(&reader, pos);

  mkvparser::Segment* parser_segment_param;
  long long ret = mkvparser::Segment::CreateInstance(&reader,
                                                     pos,
                                                     parser_segment_param);
  parser_segment.reset(parser_segment_param);
  if (ret) {
    printf("\n Segment::CreateInstance() failed.");
    return false;
  }

  ret = parser_segment->Load();
  if (ret < 0) {
    printf("\n Segment::Load() failed.");
    return false;
  }

  const mkvparser::SegmentInfo* const segment_info = parser_segment->GetInfo();
  const long long timeCodeScale = segment_info->GetTimeCodeScale();

  // Set muxer header info
  if (!writer.Open(output.c_str())) {
    printf("\n Filename is invalid or error while opening.\n");
    return false;
  }

  muxer_segment.reset(new mkvmuxer::Segment(&writer));
  muxer_segment->set_mode(mkvmuxer::Segment::kFile);

  // Set SegmentInfo element attributes
  mkvmuxer::SegmentInfo* const info = muxer_segment->GetSegmentInfo();
  info->set_timecode_scale(timeCodeScale);
  info->set_writing_app("webm_crypt");

  return true;
}

void GenerateKeyCheckFile(const std::string& key_file,
                          scoped_ptr<crypto::SymmetricKey>& key) {
  if (!key_file.empty()) {
    FILE* f;
    f = fopen(key_file.c_str(), "rb");
    if (f) {
      fseek(f, 0, SEEK_END);
      const int key_size = ftell(f);
      fseek(f, 0, SEEK_SET);

      std::string raw_key;
      raw_key.resize(key_size);
      char* raw_key_buf = const_cast<char*>(raw_key.data());
      const int bytes_read = fread(raw_key_buf, 1, key_size, f);
      if (bytes_read != key_size) {
        printf("\n Could not read key file.\n Generating key.");

        key.reset(crypto::SymmetricKey::GenerateRandomKey(
                      crypto::SymmetricKey::AES, 128));
      } else {
        key.reset(crypto::SymmetricKey::Import(crypto::SymmetricKey::AES,
                                               raw_key));
      }

      fclose(f);
    } else {
      printf("\n Could not read key file.\n Generating key.");

      key.reset(crypto::SymmetricKey::GenerateRandomKey(
                    crypto::SymmetricKey::AES, 128));
    }
  } else {
    printf("\n Key file is empty.\n Generating key.");

    key.reset(crypto::SymmetricKey::GenerateRandomKey(
              crypto::SymmetricKey::AES, 128));
  }
}

void GenerateKeyFromContentID(const mkvparser::ContentEncoding* const encoding,
                              scoped_ptr<crypto::SymmetricKey>& key) {
  // Check to see if there is a content id and assume that is the
  // encryption key.
  // Only check the first content encoding.
  assert(encoding);

  if (encoding->GetEncryptionCount() > 0) {
    // Only check the first encryption.
    const mkvparser::ContentEncoding::ContentEncryption* const encryption =
        encoding->GetEncryptionByIndex(0);
    assert(encryption);

    if (encryption->key_id_len > 0) {
      std::string raw_key;
      raw_key.resize(encryption->key_id_len);
      char* raw_key_buf = const_cast<char*>(raw_key.data());
      memcpy(raw_key_buf, encryption->key_id, encryption->key_id_len);

      key.reset(crypto::SymmetricKey::Import(crypto::SymmetricKey::AES,
                                             raw_key));
    }
  }
}

bool ReadKeyFromFile(const std::string& key_file,
                     scoped_ptr<crypto::SymmetricKey>& key) {
  FILE* f;
  f = fopen(key_file.c_str(), "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    const int key_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string raw_key;
    raw_key.resize(key_size);
    char* raw_key_buf = const_cast<char*>(raw_key.data());
    const int bytes_read = fread(raw_key_buf, 1, key_size, f);
    if (bytes_read != key_size) {
      printf("\n Could not read key file.\n");
      return false;
    }

    key.reset(crypto::SymmetricKey::Import(crypto::SymmetricKey::AES,
                                           raw_key));
    fclose(f);
  } else {
    printf("\n Could not read key file.\n");
    return false;
  }

  return true;
}

bool DecryptDataInPlace(const scoped_ptr<crypto::SymmetricKey>& key,
                        const unsigned char* const data,
                        int length,
                        std::string& decrypttext) {
  // Initialize encryption data.
  // The IV must be exactly as long as the cipher block size.
  const std::string kInitialCounter = "0000000000000000";
  crypto::Encryptor encryptor;

  if (!encryptor.Init(key.get(),
      crypto::Encryptor::CBC,
      kInitialCounter)) {
      printf("\n Could not initialize encryptor.\n");
      return false;
  }

  std::string encryptedtext;
  encryptedtext.resize(length);
  char* raw_buf = const_cast<char*>(encryptedtext.data());
  memcpy(raw_buf, data, length);

  if (!encryptor.Decrypt(encryptedtext, &decrypttext)) {
    printf("\n Could not decrypt data.\n");
    return false;
  }

  return true;
}

bool EncryptDataInPlace(const scoped_ptr<crypto::SymmetricKey>& key,
                        const unsigned char* const data,
                        int length,
                        std::string& ciphertext) {
  // Initialize encryption data.
  // The IV must be exactly as long as the cipher block size.
  const std::string kInitialCounter = "0000000000000000";
  crypto::Encryptor encryptor;

  if (!encryptor.Init(key.get(),
      crypto::Encryptor::CBC,
      kInitialCounter)) {
      printf("\n Could not initialize encryptor.\n");
      return false;
  }

  std::string plaintext;
  plaintext.resize(length);
  char* raw_buf = const_cast<char*>(plaintext.data());
  memcpy(raw_buf, data, length);

  if (!encryptor.Encrypt(plaintext, &ciphertext)) {
    printf("\n Could not encrypt data.\n");
    return false;
  }

  return true;
}

int WebMEncrypt(std::string input,
                std::string output,
                std::string key_file,
                std::string content_id,
                bool encrypt_video,
                bool encrypt_audio) {
  scoped_ptr<crypto::SymmetricKey> key;

  GenerateKeyCheckFile(key_file, key);

  // Get parser header info
  mkvparser::MkvReader reader;
  scoped_ptr<mkvparser::Segment> parser_segment;
  mkvmuxer::MkvWriter writer;
  scoped_ptr<mkvmuxer::Segment> muxer_segment;

  if (!OpenWebMFiles(reader,
                     input,
                     parser_segment,
                     writer,
                     output,
                     muxer_segment)) {
    printf("\n Could not open WebM files.\n");
    return -1;
  }

  // Set Tracks element attributes
  enum { kVideoTrack = 1, kAudioTrack = 2 };
  const mkvparser::Tracks* const parser_tracks = parser_segment->GetTracks();
  unsigned long i = 0;
  uint64 vid_track = 0; // no track added
  uint64 aud_track = 0; // no track added

  while (i != parser_tracks->GetTracksCount()) {
    int track_num = i++;

    const mkvparser::Track* const parser_track =
        parser_tracks->GetTrackByIndex(track_num);

    if (parser_track == NULL)
      continue;

    const char* const track_name = parser_track->GetNameAsUTF8();
    const long long track_type = parser_track->GetType();

    if (track_type == kVideoTrack) {
      // Get the video track from the parser
      const mkvparser::VideoTrack* const pVideoTrack =
          static_cast<const mkvparser::VideoTrack*>(parser_track);
      const long long width =  pVideoTrack->GetWidth();
      const long long height = pVideoTrack->GetHeight();

      // Add the video track to the muxer. 0 tells muxer to decide on track
      // number.
      vid_track = muxer_segment->AddVideoTrack(static_cast<int>(width),
                                              static_cast<int>(height),
                                              0);
      if (!vid_track) {
        printf("\n Could not add video track.\n");
        return -1;
      }

      mkvmuxer::VideoTrack* const video =
          static_cast<mkvmuxer::VideoTrack*>(
              muxer_segment->GetTrackByNumber(vid_track));
      if (!video) {
        printf("\n Could not get video track.\n");
        return -1;
      }

      if (track_name)
        video->set_name(track_name);

      const double rate = pVideoTrack->GetFrameRate();
      if (rate > 0.0) {
        video->set_frame_rate(rate);
      }

      if (encrypt_video) {
        if (!video->AddContentEncoding()) {
          printf("\n Could not add ContentEncoding to video track.\n");
          return -1;
        }

        mkvmuxer::ContentEncoding* const encoding =
            video->GetContentEncodingByIndex(0);
        if (!encoding) {
          printf("\n Could not add ContentEncoding to video track.\n");
          return -1;
        }

        if (content_id.empty()) {
          // If the content id is empty set the content id to the raw key.
          // TODO(fgalligan): This feature may need to be taken out later.
          std::string raw_key;
          if (!key->GetRawKey(&raw_key)) {
            printf("Could not get raw key to set for contnent id.\n");
            return -1;
          }
          const unsigned char* const data =
              reinterpret_cast<const unsigned char*>(raw_key.data());
          if (!encoding->SetEncryptionID(data, raw_key.size())) {
            printf("\n Could not set encryption id for video track.\n");
            return -1;
          }
        } else {
          const unsigned char* const data =
              reinterpret_cast<const unsigned char*>(content_id.data());
          if (!encoding->SetEncryptionID(data, content_id.size())) {
            printf("\n Could not set encryption id for video track.\n");
            return -1;
          }
        }
      }
    } else if (track_type == kAudioTrack) {
      // Get the audio track from the parser
      const mkvparser::AudioTrack* const pAudioTrack =
          static_cast<const mkvparser::AudioTrack*>(parser_track);
      const long long channels =  pAudioTrack->GetChannels();
      const double sample_rate = pAudioTrack->GetSamplingRate();

      // Add the audio track to the muxer. 0 tells muxer to decide on track
      // number.
      aud_track = muxer_segment->AddAudioTrack(static_cast<int>(sample_rate),
                                              static_cast<int>(channels),
                                              0);
      if (!aud_track) {
        printf("\n Could not add audio track.\n");
        return -1;
      }

      mkvmuxer::AudioTrack* const audio =
          static_cast<mkvmuxer::AudioTrack*>(
              muxer_segment->GetTrackByNumber(aud_track));
      if (!audio) {
        printf("\n Could not get audio track.\n");
        return -1;
      }

      if (track_name)
        audio->set_name(track_name);

      size_t private_size;
      const unsigned char* const private_data =
          pAudioTrack->GetCodecPrivate(private_size);
      if (private_size > 0) {
        if (!audio->SetCodecPrivate(private_data, private_size)) {
          printf("\n Could not add audio private data.\n");
          return -1;
        }
      }

      const long long bit_depth = pAudioTrack->GetBitDepth();
      if (bit_depth > 0)
        audio->set_bit_depth(bit_depth);

      if (encrypt_audio) {
        if (!audio->AddContentEncoding()) {
          printf("\n Could not add ContentEncoding to audio track.\n");
          return -1;
        }

        mkvmuxer::ContentEncoding* const encoding =
            audio->GetContentEncodingByIndex(0);
        if (!encoding) {
          printf("\n Could not add ContentEncoding to audio track.\n");
          return -1;
        }

        if (content_id.empty()) {
          // If the content id is empty set the content id to the raw key.
          // TODO(fgalligan): This feature may need to be taken out later.
          std::string raw_key;
          if (!key->GetRawKey(&raw_key)) {
            printf("Could not get raw key to set for contnent id.\n");
            return -1;
          }
          const unsigned char* const data =
              reinterpret_cast<const unsigned char*>(raw_key.data());
          if (!encoding->SetEncryptionID(data, raw_key.size())) {
            printf("\n Could not set encryption id for audio track.\n");
            return -1;
          }
        } else {
          const unsigned char* const data =
              reinterpret_cast<const unsigned char*>(content_id.data());
          if (!encoding->SetEncryptionID(data, content_id.size())) {
            printf("\n Could not set encryption id for audio track.\n");
            return -1;
          }
        }
      }
    }
  }

  // Set Cues element attributes
  muxer_segment->CuesTrack(vid_track);

  // Write clusters
  scoped_ptr<unsigned char> data;
  int data_len = 0;

  const mkvparser::Cluster* cluster = parser_segment->GetFirst();

  while ((cluster != NULL) && !cluster->EOS()) {
    const mkvparser::BlockEntry* block_entry = cluster->GetFirst();

    while ((block_entry != NULL) && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      const long long trackNum = block->GetTrackNumber();
      const mkvparser::Track* const parser_track =
          parser_tracks->GetTrackByNumber(
              static_cast<unsigned long>(trackNum));
      const long long track_type = parser_track->GetType();

      if ((track_type == kAudioTrack) ||
          (track_type == kVideoTrack)) {
        const int frame_count = block->GetFrameCount();
        const long long time_ns = block->GetTime(cluster);
        const bool is_key = block->IsKey();

        for (int i = 0; i < frame_count; ++i) {
          const mkvparser::Block::Frame& frame = block->GetFrame(i);

          // Add 16 for padding.
          if (frame.len + 16 > data_len) {
            data.reset(new unsigned char[frame.len + 16]);
            if (!data.get())
              return -1;
            data_len = frame.len + 16;
          }

          if (frame.Read(&reader, data.get()))
            return -1;

          uint64 track_num = vid_track;
          if (track_type == kAudioTrack)
            track_num = aud_track;

          if ( (track_type == kVideoTrack && encrypt_video) ||
              (track_type == kAudioTrack && encrypt_audio) ) {
            std::string ciphertext;
            if (!EncryptDataInPlace(key, data.get(), frame.len, ciphertext)) {
              printf("\n Could encrypt data.\n");
              return -1;
            }

            unsigned char* encrypted_data =
                reinterpret_cast<unsigned char*>(
                    const_cast<char*>(ciphertext.data()));
            if (!muxer_segment->AddFrame(encrypted_data,
                                        ciphertext.size(),
                                        track_num,
                                        time_ns,
                                        is_key)) {
              printf("\n Could not add encrypted frame.\n");
              return -1;
            }

          } else {
            if (!muxer_segment->AddFrame(data.get(),
                                        frame.len,
                                        track_num,
                                        time_ns,
                                        is_key)) {
              printf("\n Could not add frame.\n");
              return -1;
            }
          }
        }
      }

      block_entry = cluster->GetNext(block_entry);
    }

    cluster = parser_segment->GetNext(cluster);
  }

  muxer_segment->Finalize();

  std::string key_output_file;
  if (!key_file.empty()) {
    FILE* f;
    f = fopen(key_file.c_str(), "rb");
    if (f) {
      // Key file was passed in.
      fclose(f);
    } else {
      key_output_file = key_file;
    }
  } else {
    key_output_file = "aes.key";
  }

  // Output the key.
  if (!key_output_file.empty()) {
    FILE* f;
    f = fopen(key_output_file.c_str(), "wb");
    if (!f) {
      printf("\n Could not open key_file: %s for writing.\n",
             key_output_file.c_str());
      return -1;
    }

    std::string raw_key;
    if (!key->GetRawKey(&raw_key)) {
      printf("Could not get raw key.\n");
      return -1;
    }

    const int bytes_written = fwrite(raw_key.data() ,1 , raw_key.size(), f);
    fclose(f);

    if (bytes_written != static_cast<int>(raw_key.size())) {
      printf("Error writing to key file.\n");
      return -1;
    }
  }

  writer.Close();
  reader.Close();

  return true;
}

int WebMDecrypt(std::string input,
                std::string output,
                std::string key_file,
                bool decrypt_video,
                bool decrypt_audio) {
  scoped_ptr<crypto::SymmetricKey> key;

  // If the key_file is empty assume the key is the Content ID.
  if (!key_file.empty()) {
    if (!ReadKeyFromFile(key_file, key))
      return -1;
  }

  mkvparser::MkvReader reader;
  scoped_ptr<mkvparser::Segment> parser_segment;
  mkvmuxer::MkvWriter writer;
  scoped_ptr<mkvmuxer::Segment> muxer_segment;

  if (!OpenWebMFiles(reader,
                     input,
                     parser_segment,
                     writer,
                     output,
                     muxer_segment)) {
    printf("\n Could not open WebM files.\n");
    return -1;
  }

  // Set Tracks element attributes
  enum { kVideoTrack = 1, kAudioTrack = 2 };
  const mkvparser::Tracks* const parser_tracks = parser_segment->GetTracks();
  unsigned long i = 0;
  uint64 vid_track = 0; // no track added
  uint64 aud_track = 0; // no track added

  while (i != parser_tracks->GetTracksCount()) {
    int track_num = i++;

    const mkvparser::Track* const parser_track =
        parser_tracks->GetTrackByIndex(track_num);

    if (parser_track == NULL)
      continue;

    const char* const track_name = parser_track->GetNameAsUTF8();
    const long long track_type = parser_track->GetType();

    if (track_type == kVideoTrack) {
      // Get the video track from the parser
      const mkvparser::VideoTrack* const pVideoTrack =
          static_cast<const mkvparser::VideoTrack*>(parser_track);
      const long long width =  pVideoTrack->GetWidth();
      const long long height = pVideoTrack->GetHeight();

      // Add the video track to the muxer. 0 tells muxer to decide on track
      // number.
      vid_track = muxer_segment->AddVideoTrack(static_cast<int>(width),
                                              static_cast<int>(height),
                                              0);
      if (!vid_track) {
        printf("\n Could not add video track.\n");
        return -1;
      }

      mkvmuxer::VideoTrack* const video =
          static_cast<mkvmuxer::VideoTrack*>(
              muxer_segment->GetTrackByNumber(vid_track));
      if (!video) {
        printf("\n Could not get video track.\n");
        return -1;
      }

      if (track_name)
        video->set_name(track_name);

      const double rate = pVideoTrack->GetFrameRate();
      if (rate > 0.0) {
        video->set_frame_rate(rate);
      }
    } else if (track_type == kAudioTrack) {
      // Get the audio track from the parser
      const mkvparser::AudioTrack* const pAudioTrack =
          static_cast<const mkvparser::AudioTrack*>(parser_track);
      const long long channels =  pAudioTrack->GetChannels();
      const double sample_rate = pAudioTrack->GetSamplingRate();

      // Add the audio track to the muxer. 0 tells muxer to decide on track
      // number.
      aud_track = muxer_segment->AddAudioTrack(static_cast<int>(sample_rate),
                                              static_cast<int>(channels),
                                              0);
      if (!aud_track) {
        printf("\n Could not add audio track.\n");
        return -1;
      }

      mkvmuxer::AudioTrack* const audio =
          static_cast<mkvmuxer::AudioTrack*>(
              muxer_segment->GetTrackByNumber(aud_track));
      if (!audio) {
        printf("\n Could not get audio track.\n");
        return -1;
      }

      if (track_name)
        audio->set_name(track_name);

      size_t private_size;
      const unsigned char* const private_data =
          pAudioTrack->GetCodecPrivate(private_size);
      if (private_size > 0) {
        if (!audio->SetCodecPrivate(private_data, private_size)) {
          printf("\n Could not add audio private data.\n");
          return -1;
        }
      }

      const long long bit_depth = pAudioTrack->GetBitDepth();
      if (bit_depth > 0)
        audio->set_bit_depth(bit_depth);
    }

    const int encoding_count = parser_track->GetContentEncodingCount();
    if (encoding_count > 0 && key_file.empty()) {
      // Check to see if there is a content id and assume that is the
      // encryption key.
      // Only check the first content encoding.
      const mkvparser::ContentEncoding* const encoding =
          parser_track->GetContentEncodingByIndex(0);

      GenerateKeyFromContentID(encoding, key);
    }
  }

  // Set Cues element attributes
  muxer_segment->CuesTrack(vid_track);

  // Write clusters
  scoped_ptr<unsigned char> data;
  int data_len = 0;

  const mkvparser::Cluster* cluster = parser_segment->GetFirst();

  while ((cluster != NULL) && !cluster->EOS()) {
    const mkvparser::BlockEntry* block_entry = cluster->GetFirst();

    while ((block_entry != NULL) && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      const long long trackNum = block->GetTrackNumber();
      const mkvparser::Track* const parser_track =
          parser_tracks->GetTrackByNumber(
              static_cast<unsigned long>(trackNum));
      const long long track_type = parser_track->GetType();

      if ((track_type == kAudioTrack) ||
          (track_type == kVideoTrack)) {
        const int frame_count = block->GetFrameCount();
        const long long time_ns = block->GetTime(cluster);
        const bool is_key = block->IsKey();

        for (int i = 0; i < frame_count; ++i) {
          const mkvparser::Block::Frame& frame = block->GetFrame(i);

          // Add 16 for padding.
          if (frame.len + 16 > data_len) {
            data.reset(new unsigned char[frame.len + 16]);
            if (!data.get())
              return -1;
            data_len = frame.len + 16;
          }

          if (frame.Read(&reader, data.get()))
            return -1;

          uint64 track_num = vid_track;
          if (track_type == kAudioTrack)
            track_num = aud_track;

          if ( (track_type == kVideoTrack && decrypt_video) ||
              (track_type == kAudioTrack && decrypt_audio) ) {

            std::string decrypttext;
            if (!DecryptDataInPlace(key, data.get(), frame.len, decrypttext)) {
              printf("\n Could not decrypt data.\n");
              return -1;
            }

            unsigned char* decrypted_data =
                reinterpret_cast<unsigned char*>(
                    const_cast<char*>(decrypttext.data()));

            if (!muxer_segment->AddFrame(decrypted_data,
                                        decrypttext.size(),
                                        track_num,
                                        time_ns,
                                        is_key)) {
              printf("\n Could not add encrypted frame.\n");
              return -1;
            }

          } else {
            if (!muxer_segment->AddFrame(data.get(),
                                        frame.len,
                                        track_num,
                                        time_ns,
                                        is_key)) {
              printf("\n Could not add frame.\n");
              return -1;
            }
          }
        }
      }

      block_entry = cluster->GetNext(block_entry);
    }

    cluster = parser_segment->GetNext(cluster);
  }

  muxer_segment->Finalize();

  writer.Close();
  reader.Close();

  return true;
}

} //end namespace

// These values are not getting compiled into base.lib.
namespace switches {

// Gives the default maximal active V-logging level; 0 is the default.
// Normally positive values are used for V-logging levels.
const char kV[]                             = "v";

// Gives the per-module maximal V-logging levels to override the value
// given by --v.  E.g. "my_module=2,foo*=3" would change the logging
// level for all code in source files "my_module.*" and "foo*.*"
// ("-inl" suffixes are also disregarded for this matching).
//
// Any pattern containing a forward or backward slash will be tested
// against the whole pathname and not just the module.  E.g.,
// "*/foo/bar/*=2" would change the logging level for all code in
// source files under a "foo/bar" directory.
const char kVModule[]                       = "vmodule";

}  // namespace switches

int main(int argc, char* argv[]) {
  std::string input;
  std::string output;
  std::string key_file;
  std::string content_id;
  bool video = true;
  bool audio = false;
  bool encrypt = true;
  bool test = false;

  // Parse command line options.
  for (int i = 1; i < argc; ++i) {
    char* end;

    if (!strcmp("-h", argv[i]) || !strcmp("-?", argv[i])) {
      Usage();
      return 0;
    } else if (!strcmp("-i", argv[i])) {
      input = argv[++i];
    } else if (!strcmp("-o", argv[i])) {
      output = argv[++i];
    } else if (!strcmp("-key_file", argv[i])) {
      key_file = argv[++i];
    } else if (!strcmp("-content_id", argv[i])) {
      content_id = argv[++i];
    } else if (!strcmp("-audio", argv[i])) {
      audio = (strtol(argv[++i], &end, 10) == 0) ? false : true;
    } else if (!strcmp("-video", argv[i])) {
      video = (strtol(argv[++i], &end, 10) == 0) ? false : true;
    } else if (!strcmp("-encrypt", argv[i])) {
      encrypt = (strtol(argv[++i], &end, 10) == 0) ? false : true;
    } else if (!strcmp("-test", argv[i])) {
      test = true;
    }
  }

  if (test) {
    TestEncryption();
    return 0;
  }

  if (input.empty()) {
    printf("No input file set.\n");
    Usage();
    return -1;
  }
  if (output.empty()) {
    printf("No output file set.\n");
    Usage();
    return -1;
  }

  if (encrypt) {
    int rv = WebMEncrypt(input, output, key_file, content_id, video, audio);
    if (rv < 0) {
      printf("Error encrypting WebM file.\n");
      return rv;
    }
  } else {
    int rv = WebMDecrypt(input, output, key_file, video, audio);
    if (rv < 0) {
      printf("Error decrypting WebM file.\n");
      return rv;
    }
  }

  return 0;
}
