// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <cstdlib>
#include <ctime>
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

// This application uses the crypto library from the Chromium project. See the
// readme.txt for build instructions.

namespace {

using crypto::Encryptor;
using crypto::SymmetricKey;
using mkvparser::ContentEncoding;
using std::string;

static const char WEBM_CRYPT_VERSION_STRING[] = "0.1.0.0";

// Struct to hold encryption settings for a single WebM stream.
struct EncryptionSettings {
  EncryptionSettings()
      : cipher_mode("CTR"),
        content_id(),
        init_data("0000"),
        key_file(),
        key_password(),
        key_salt() {
  }

  // AES encryption algorithm. Currently only "CTR" is supported.
  string cipher_mode;

  // WebM Content ID element.
  string content_id;

  // AES encryption initialization data. For "CTR" this is a Nonce.
  string init_data;

  // Path to a file which holds the encryption key.
  string key_file;

  // Password used to generate an encryption key.
  string key_password;

  // Salt used to generate an encryption key.
  string key_salt;
};

// Struct to hold file wide encryption settings.
struct WebMCryptSettings {
  WebMCryptSettings()
      : input(),
        output(),
        video(true),
        audio(false),
        aud_enc(),
        vid_enc() {
  }

  // Path to input file.
  string input;

  // Path to output file.
  string output;

  // Flag telling if the video stream should be encrypted.
  bool video;

  // Flag telling if the audio stream should be encrypted.
  bool audio;

  // Encryption settings for the audio stream.
  EncryptionSettings aud_enc;

  // Encryption settings for the video stream.
  EncryptionSettings vid_enc;
};

// Class to encrypt data for one WebM stream according to the WebM encryption
// RFC specification.
class EncryptModule {
 public:
  static const int kKeySize = 16;
  static const unsigned int kNonceSize = 4;
  static const int kIntegrityCheckSize = 12;

  // |enc| Encryption settings for a stream. |key| Encryption key for a
  // stream and is owned by this class.
  EncryptModule(const EncryptionSettings& enc, SymmetricKey* key);
  ~EncryptModule();

  // Encrypts |data| according to the encryption settings and encryption key.
  // |length| is the size of |data| in bytes. |time_ns| is the starting time
  // of |data| in nanoseconds. |ciphertext| is the returned encrypted data.
  // Returns true if |data| was encrypted and passed back through |ciphertext|.
  bool EncryptData(const uint8* data,
                   size_t length,
                   int64 time_ns,
                   string* ciphertext);

  // Generates a 16 byte CTR Counter Block. The format is
  // | nonce | iv | block counter |. |nonce| is a 4 byte CTR nonce. |iv| is
  // an 8 byte CTR IV. |counter_block| is an output string containing the
  // Counter Block. Returns true on success.
  static bool GenerateCounterBlock(const string& nonce,
                                   int64 iv,
                                   string* counter_block);

 private:
  // Encryption settings for the stream.
  const EncryptionSettings enc_;

  // Encryption key for the stream.
  const scoped_ptr<SymmetricKey> key_;

  // The last IV used.
  int64 last_iv_;
};

EncryptModule::EncryptModule(const EncryptionSettings& enc, SymmetricKey* key)
    : enc_(enc),
      key_(key),
      last_iv_(-1) {
}

EncryptModule::~EncryptModule() {}

bool EncryptModule::EncryptData(const uint8* data,
                                size_t length,
                                int64 time_ns,
                                string* ciphertext) {
  if (!ciphertext)
    return false;

  if (enc_.init_data.length() != kNonceSize) {
    printf("\n Invalid IV size.\n");
    return false;
  }

  Encryptor encryptor;
  if (!encryptor.Init(key_.get(), Encryptor::CTR, "")) {
    printf("\n Could not initialize encryptor.\n");
    return false;
  }

  // Set the IV.
  const int64 iv = (time_ns <= last_iv_) ? last_iv_ + 1 : time_ns;
  last_iv_ = iv;

  string counter_block;
  if (!EncryptModule::GenerateCounterBlock(enc_.init_data,
                                           iv,
                                           &counter_block)) {
    printf("\n Could not generate counter block.\n");
    return false;
  }

  if (!encryptor.SetCounter(counter_block)) {
    printf("\n Could not set counter.\n");
    return false;
  }

  string plaintext(reinterpret_cast<const char*>(data), length);
  if (!encryptor.Encrypt(plaintext, ciphertext)) {
    printf("\n Could not encrypt data.\n");
    return false;
  }

  // Prepend a dummy integrity check.
  ciphertext->insert(0, kIntegrityCheckSize, '0');
  return true;
}

bool EncryptModule::GenerateCounterBlock(const string& nonce,
                                         int64 iv,
                                         string* counter_block) {
  if (!counter_block)
    return false;
  char temp[kKeySize];

  // Set the nonce.
  memcpy(temp, nonce.data(), kNonceSize);
  uint32 offset = kNonceSize;

  // Set the IV.
  memcpy(temp + offset, &iv, sizeof(iv));
  offset += sizeof(iv);

  // Set block counter to all 0's.
  memset(temp + offset, 0, kKeySize - offset);

  counter_block->assign(temp, sizeof(temp));
  return true;
}

// Class to decrypt data for one WebM stream according to the WebM encryption
// RFC specification.
class DecryptModule {
 public:
  static const int kKeySize = 16;
  static const unsigned int kNonceSize = 4;
  static const int kIntegrityCheckSize = 12;

  DecryptModule(const EncryptionSettings& enc, SymmetricKey* key);
  ~DecryptModule();

  // Initializes and Checks the one time decryption data. Returns true on
  // success.
  bool Init();

  // Decrypts |data| according to the encryption settings and encryption key.
  // |length| is the size of |data| in bytes. |time_ns| is the starting time
  // of |data| in nanoseconds. |decrypttext| is the returned decrypted data.
  // Returns true if |data| was decrypted and passed back through
  // |decrypttext|.
  bool DecryptData(const uint8* data,
                   size_t length,
                   int64 time_ns,
                   string* decrypttext);

 private:
  // Encryption settings for the stream.
  const EncryptionSettings enc_;

  // Encryption key for the stream.
  const scoped_ptr<SymmetricKey> key_;

  // Decryption class.
  Encryptor encryptor_;

  // The last IV used.
  int64 last_iv_;
};

DecryptModule::DecryptModule(const EncryptionSettings& enc,
                             SymmetricKey* key)
    : enc_(enc),
      key_(key),
      encryptor_(),
      last_iv_(-1) {
}

DecryptModule::~DecryptModule() {}

bool DecryptModule::Init() {
  if (!key_.get()) {
    printf("\n Could not initialize decryptor. Key was NULL.\n");
    return false;
  }

  if (enc_.init_data.length() != kNonceSize) {
    printf("\n Invalid IV size.\n");
    return false;
  }

  if (!encryptor_.Init(key_.get(), Encryptor::CTR, "")) {
    printf("\n Could not initialize decryptor.\n");
    return false;
  }
  return true;
}

bool DecryptModule::DecryptData(const uint8* data,
                                size_t length,
                                int64 time_ns,
                                string* decrypttext) {
  if (!decrypttext)
    return false;

  // Set the IV.
  const int64 iv = (time_ns <= last_iv_) ? last_iv_ + 1 : time_ns;
  last_iv_ = iv;

  string counter_block;
  if (!EncryptModule::GenerateCounterBlock(enc_.init_data,
                                           iv,
                                           &counter_block)) {
    printf("\n Could not generate counter block.\n");
    return false;
  }

  if (!encryptor_.SetCounter(counter_block)) {
    printf("\n Could not set counter.\n");
    return false;
  }

  // Skip past the integrity check.
  string encryptedtext(
      reinterpret_cast<const char*>(data + kIntegrityCheckSize),
      length - kIntegrityCheckSize);
  if (!encryptor_.Decrypt(encryptedtext, decrypttext)) {
    printf("\n Could not decrypt data.\n");
    return false;
  }
  return true;
}

void Usage() {
  printf("Usage: webm_crypt [-test] -i <input> -o <output> [Main options] "
         "[audio options] [video options]\n");
  printf("\n");
  printf("Main options:\n");
  printf("  -h | -?              Show help.\n");
  printf("  -v                   Show version\n");
  printf("  -test                Tests the encryption and decryption.\n");
  printf("  -audio <bool>        Process audio stream. (Default false)\n");
  printf("  -video <bool>        Process video stream. (Default true)\n");
  printf("  -decrypt             Decrypt the stream. (Default encrypt)\n");
  printf("  \n");
  printf("-audio_options <string> Semicolon separated name value pair.\n");
  printf("  key_file=<string>     Path to key file. (Default empty)\n");
  printf("  content_id=<string>   Encryption content ID. (Default empty)\n");
  printf("  key_password=<string> Password used to generate a key.(Default\n");
  printf("                        empty)\n");
  printf("  key_salt=<string>     Salt used to generate a key. (Default\n");
  printf("                        empty)\n");
  printf("  init_data=<string>    Init data value. (Default 0000)\n");
  printf("  \n");
  printf("-video_options <string> Semicolon separated name value pair.\n");
  printf("  key_file=<string>     Path to key file. (Default empty)\n");
  printf("  content_id=<string>   Encryption content ID. (Default empty)\n");
  printf("  key_password=<string> Password used to generate a key.(Default\n");
  printf("                        empty)\n");
  printf("  key_salt=<string>     Salt used to generate a key. (Default\n");
  printf("                        empty)\n");
  printf("  init_data=<string>    Init data value. (Default 0000)\n");
}

void TestEncryption() {
  using std::cout;
  using std::endl;

  scoped_ptr<crypto::SymmetricKey> sym_key(
      crypto::SymmetricKey::DeriveKeyFromPassword(
          crypto::SymmetricKey::AES, "password", "saltiest", 1000, 256));

  string raw_key;
  if (!sym_key->GetRawKey(&raw_key)) {
    printf("Could not get raw key.\n");
    return;
  }

  const string kInitialCounter(16, '0');

  Encryptor encryptor;
  // The IV must be exactly as long as the cipher block size.
  bool b = encryptor.Init(sym_key.get(), Encryptor::CTR, "");
  if (!b) {
    printf("Could not initialize encrypt object.\n");
    return;
  }
  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    printf("Could not SetCounter on encryptor.\n");
    return;
  }

  string plaintext("this is the plaintext");
  string ciphertext;
  b = encryptor.Encrypt(plaintext, &ciphertext);
  if (!b) {
    printf("Could not encrypt data.\n");
    return;
  }

  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    printf("Could not SetCounter on encryptor.\n");
    return;
  }

  string decrypted;
  b = encryptor.Decrypt(ciphertext, &decrypted);
  if (!b) {
    printf("Could not decrypt data.\n");
    return;
  }

  printf("Test 1 finished.\n");
  cout << "iv         :" << kInitialCounter << endl;
  cout << "raw_key    :" << raw_key << endl;
  cout << "plaintext  :" << plaintext << endl;
  cout << "ciphertext :" << ciphertext << endl;
  cout << "decrypted  :" << decrypted << endl;

  const int kNonAsciiSize = 256;
  uint8 raw_data[kNonAsciiSize];
  for (int i = 0; i < kNonAsciiSize; ++i) {
    raw_data[i] = i;
  }
  string non_ascii(reinterpret_cast<char*>(raw_data), kNonAsciiSize);

  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    printf("Could not SetCounter on encryptor.\n");
    return;
  }

  b = encryptor.Encrypt(non_ascii, &ciphertext);
  if (!b) {
    printf("Could not encrypt data.\n");
    return;
  }

  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    printf("Could not SetCounter on encryptor.\n");
    return;
  }

  b = encryptor.Decrypt(ciphertext, &decrypted);
  if (!b) {
    printf("Could not decrypt data.\n");
    return;
  }

  printf("Test 2 finished.\n");
  cout << "iv         :" << kInitialCounter << endl;
  cout << "non_ascii  :" << non_ascii << endl;
  cout << "ciphertext :" << ciphertext << endl;
  cout << "decrypted  :" << decrypted << endl;

  non_ascii.append(plaintext);

  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    printf("Could not SetCounter on encryptor.\n");
    return;
  }

  b = encryptor.Encrypt(non_ascii, &ciphertext);
  if (!b) {
    printf("Could not encrypt data.\n");
    return;
  }

  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    printf("Could not SetCounter on encryptor.\n");
    return;
  }

  b = encryptor.Decrypt(ciphertext, &decrypted);
  if (!b) {
    printf("Could not decrypt data.\n");
    return;
  }

  printf("Test 3 finished.\n");
  cout << "iv         :" << kInitialCounter << endl;
  cout << "non_ascii  :" << non_ascii << endl;
  cout << "ciphertext :" << ciphertext << endl;
  cout << "decrypted  :" << decrypted << endl;
  printf("Tests passed.\n");
}

// Opens and initializes the input and output WebM files. |input| path to the
// input WebM file. |output| path to the output WebM file. |reader| WebM
// reader class output parameter. |parser_segment| WebM parser class output
// parameter. |writer| WebM writer class output parameter. |muxer_segment|
// WebM muxer class output parameter.
bool OpenWebMFiles(const string& input,
                   const string& output,
                   mkvparser::MkvReader* reader,
                   mkvparser::Segment** parser_segment,
                   mkvmuxer::MkvWriter* writer,
                   mkvmuxer::Segment** muxer_segment) {
  if (!reader || !parser_segment || !writer || !muxer_segment)
    return false;

  if (reader->Open(input.c_str())) {
    printf("\n Filename is invalid or error while opening.\n");
    return false;
  }

  long long pos = 0;  // NOLINT
  mkvparser::EBMLHeader ebml_header;
  ebml_header.Parse(reader, pos);

  mkvparser::Segment*& parser = *parser_segment;
  int64 ret = mkvparser::Segment::CreateInstance(reader, pos, parser);
  if (ret) {
    printf("\n Segment::CreateInstance() failed.");
    return false;
  }

  ret = parser->Load();
  if (ret < 0) {
    printf("\n Segment::Load() failed.");
    return false;
  }

  const mkvparser::SegmentInfo* const segment_info = parser->GetInfo();
  const int64 timeCodeScale = segment_info->GetTimeCodeScale();

  // Set muxer header info
  if (!writer->Open(output.c_str())) {
    printf("\n Filename is invalid or error while opening.\n");
    return false;
  }

  mkvmuxer::Segment*& muxer = *muxer_segment;
  muxer = new mkvmuxer::Segment();
  if (!muxer->Init(writer)) {
    printf("\n Could not initialize muxer segment!\n");
    return false;
  }
  muxer->set_mode(mkvmuxer::Segment::kFile);

  // Set SegmentInfo element attributes
  mkvmuxer::SegmentInfo* const info = muxer->GetSegmentInfo();
  info->set_timecode_scale(timeCodeScale);
  string app_str("webm_crypt ");
  app_str += WEBM_CRYPT_VERSION_STRING;
  info->set_writing_app(app_str.c_str());
  return true;
}

// Generates an encryption key. First checks if a path to a key file has been
// set. If not then it checks if a key password and salt have been set. If
// not it will generate an encryption key randomly. |enc| Encryption settings
// for a stream. Returns a pointer to an encryption key on success.
SymmetricKey* GenerateKeyCheckFile(const EncryptionSettings& enc) {
  SymmetricKey* key = NULL;

  if (!enc.key_file.empty()) {
    FILE* const f = fopen(enc.key_file.c_str(), "rb");
    if (f) {
      fseek(f, 0, SEEK_END);
      const int key_size = ftell(f);
      fseek(f, 0, SEEK_SET);

      scoped_array<char> raw_key_buf(new char[key_size]);
      const int bytes_read = fread(raw_key_buf.get(), 1, key_size, f);

      if (bytes_read != key_size) {
        printf("\n Could not read key file.\n Generating key.\n");
        key = SymmetricKey::GenerateRandomKey(SymmetricKey::AES, 128);
      } else {
        string raw_key(raw_key_buf.get(), bytes_read);
        key = SymmetricKey::Import(SymmetricKey::AES, raw_key);
      }

      fclose(f);
    } else {
      printf("\n Could not read key file.\n Generating key.\n");
      key = SymmetricKey::GenerateRandomKey(SymmetricKey::AES, 128);
    }
  } else if (!enc.key_password.empty() && !enc.key_salt.empty()) {
    printf("\n Generating key from password and salt.\n");
    key = SymmetricKey::DeriveKeyFromPassword(
        SymmetricKey::AES, enc.key_password, enc.key_salt, 1000, 256);
  } else {
    printf("\n Key file is empty.\n Generating key.\n");
    key = SymmetricKey::GenerateRandomKey(SymmetricKey::AES, 128);
  }
  return key;
}

// Generates an encryption key from a WebM ContentID. |encoding| is a
// ContentEncoding WebM element. Returns a pointer to an encryption key on
// success.
SymmetricKey* GenerateKeyFromContentID(const ContentEncoding& encoding) {
  SymmetricKey* key = NULL;

  // Only check the first content encoding.
  if (encoding.GetEncryptionCount() > 0) {
    // Only check the first encryption.
    const ContentEncoding::ContentEncryption* const encryption =
        encoding.GetEncryptionByIndex(0);
    if (!encryption)
      return NULL;

    if (encryption->key_id_len > 0) {
      string raw_key(reinterpret_cast<char*>(encryption->key_id),
                     static_cast<size_t>(encryption->key_id_len));
      key = SymmetricKey::Import(SymmetricKey::AES, raw_key);
    }
  }
  return key;
}

// Reads an encryption key from a file. |key_file| path to the file with the
// encryption key. Returns a pointer to an encryption key on success.
SymmetricKey* ReadKeyFromFile(const string& key_file) {
  SymmetricKey* key = NULL;

  FILE* const f = fopen(key_file.c_str(), "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    const int key_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    scoped_array<char> raw_key_buf(
        new (std::nothrow) char[key_size]);  // NOLINT
    if (!raw_key_buf.get()) {
      printf("\n Could not allocate memory for key file.\n");
      return false;
    }

    const int bytes_read = fread(raw_key_buf.get(), 1, key_size, f);
    if (bytes_read != key_size) {
      printf("\n Could not read key file.\n");
      return false;
    }

    string raw_key(raw_key_buf.get(), bytes_read);
    key = SymmetricKey::Import(SymmetricKey::AES, raw_key);
    fclose(f);
  } else {
    printf("\n Could not read key file.\n");
  }
  return key;
}

// Outputs an encryption key to a file. |enc| Encryption settings for a
// stream. |default_name| path to a file if the key file was not set in
// |enc|. |raw_key| encryption key. Returns true on success.
bool OutputKeyFile(const EncryptionSettings& enc,
                   const string& default_name,
                   const string& raw_key) {
  string key_output_file;
  if (!enc.key_file.empty()) {
    FILE* const f = fopen(enc.key_file.c_str(), "rb");
    if (f) {
      // Key file was passed in.
      fclose(f);
    } else {
      key_output_file = enc.key_file;
    }
  } else {
    key_output_file = default_name;
  }

  // Output the key.
  if (!key_output_file.empty()) {
    FILE* const f = fopen(key_output_file.c_str(), "wb");
    if (!f) {
      printf("\n Could not open key_file: %s for writing.\n",
             key_output_file.c_str());
      return false;
    }

    const size_t bytes_written = fwrite(raw_key.data(), 1, raw_key.size(), f);
    fclose(f);

    if (bytes_written != raw_key.size()) {
      printf("Error writing to key file.\n");
      return false;
    }
  }

  return true;
}

double StringToDouble(const std::string& s) {
  return strtod(s.c_str(), NULL);
}

// Parse name value pair.
bool ParseOption(const string& option, string* name, string* value) {
  if (!name || !value)
    return false;

  const size_t equal_sign = option.find_first_of('=');
  if (equal_sign == string::npos)
    return false;

  *name = option.substr(0, equal_sign);
  *value = option.substr(equal_sign + 1, string::npos);
  return true;
}

// Parse set of options for one stream.
void ParseStreamOptions(const string& option_list, EncryptionSettings* enc) {
  size_t start = 0;
  size_t end = option_list.find_first_of(',');
  for (;;) {
    const string option = option_list.substr(start, end - start);
    string name;
    string value;
    if (ParseOption(option, &name, &value)) {
      if (name == "key_file") {
        enc->key_file = value;
      } else if (name == "content_id") {
        enc->content_id = value;
      } else if (name == "key_password") {
        enc->key_password = value;
      } else if (name == "key_salt") {
        enc->key_salt = value;
      } else if (name == "init_data") {
        enc->init_data = value;
      }
    }

    if (end == string::npos)
      break;
    start = end + 1;
    end = option_list.find_first_of(',', start);
  }
}

// Parse the WebM content encryption elements. |encoding| is the WebM
// ContentEncoding elements. |enc| is the output parameter for one stream.
// Returns true on success.
bool ParseContentEncryption(const ContentEncoding& encoding,
                            EncryptionSettings* enc) {
  if (!enc)
    return false;

  if (encoding.GetEncryptionCount() > 0) {
    // Only check the first encryption.
    const ContentEncoding::ContentEncryption* const encryption =
        encoding.GetEncryptionByIndex(0);
    if (!encryption)
      return false;

    if (encryption->key_id_len > 0) {
      enc->content_id.assign(reinterpret_cast<char*>(encryption->key_id),
                             static_cast<size_t>(encryption->key_id_len));
    }

    enc->init_data.assign(
        reinterpret_cast<char*>(encryption->aes_settings.cipher_init_data),
        static_cast<uint32>(encryption->aes_settings.cipher_init_data_len));
  }

  return true;
}

// Function to encrypt a WebM file. |webm_crypt| encryption settings for
// the source and destination files. Returns 0 on success and <0 for an error.
int WebMEncrypt(const WebMCryptSettings& webm_crypt) {
  scoped_ptr<SymmetricKey> aud_key;
  scoped_ptr<SymmetricKey> vid_key;

  // Get parser header info
  mkvparser::MkvReader reader;
  mkvmuxer::MkvWriter writer;
  mkvparser::Segment* parser_segment_ptr;
  mkvmuxer::Segment* muxer_segment_ptr;
  const bool b = OpenWebMFiles(webm_crypt.input,
                               webm_crypt.output,
                               &reader,
                               &parser_segment_ptr,
                               &writer,
                               &muxer_segment_ptr);
  scoped_ptr<mkvparser::Segment> parser_segment(parser_segment_ptr);
  scoped_ptr<mkvmuxer::Segment> muxer_segment(muxer_segment_ptr);
  if (!b) {
    printf("\n Could not open WebM files.\n");
    return -1;
  }

  // Set Tracks element attributes
  enum { kVideoTrack = 1, kAudioTrack = 2 };
  const mkvparser::Tracks* const parser_tracks = parser_segment->GetTracks();
  uint32 i = 0;
  uint64 vid_track = 0;  // no track added
  uint64 aud_track = 0;  // no track added
  string aud_raw_key;
  string vid_raw_key;

  while (i != parser_tracks->GetTracksCount()) {
    const int track_num = i++;

    const mkvparser::Track* const parser_track =
        parser_tracks->GetTrackByIndex(track_num);

    if (parser_track == NULL)
      continue;

    const char* const track_name = parser_track->GetNameAsUTF8();
    const int64 track_type = parser_track->GetType();

    if (track_type == kVideoTrack) {
      // Get the video track from the parser
      const mkvparser::VideoTrack* const pVideoTrack =
          reinterpret_cast<const mkvparser::VideoTrack*>(parser_track);
      const int64 width =  pVideoTrack->GetWidth();
      const int64 height = pVideoTrack->GetHeight();

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
          reinterpret_cast<mkvmuxer::VideoTrack*>(
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

      if (webm_crypt.video) {
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

        mkvmuxer::ContentEncAESSettings* const aes =
            encoding->enc_aes_settings();
        if (!aes) {
          printf("\n Error getting video ContentEncAESSettings.\n");
          return -1;
        }
        if (!aes->SetCipherInitData(
                reinterpret_cast<const uint8*>(
                    webm_crypt.vid_enc.init_data.data()),
                webm_crypt.vid_enc.init_data.length())) {
          printf("\n Could not set Cipher Init Data.\n");
          return -1;
        }

        vid_key.reset(GenerateKeyCheckFile(webm_crypt.vid_enc));
        if (!vid_key.get()) {
          printf("\n Could not generate encryption key.\n");
          return -1;
        }

        // Copy the video raw key for later.
        if (!webm_crypt.vid_enc.key_file.empty()) {
          string raw_key;
          if (!vid_key->GetRawKey(&raw_key)) {
            printf("Could not get video raw key.\n");
            return -1;
          }
          vid_raw_key = raw_key;
        }

        if (webm_crypt.vid_enc.content_id.empty()) {
          // If the content id is empty set the content id to the raw key.
          // TODO(fgalligan): This feature may need to be taken out later.
          string raw_key;
          if (!vid_key->GetRawKey(&raw_key)) {
            printf("Could not get raw key to set for content id.\n");
            return -1;
          }

          if (!encoding->SetEncryptionID(
              reinterpret_cast<const uint8*>(raw_key.data()),
              raw_key.size())) {
            printf("\n Could not set encryption id for video track.\n");
            return -1;
          }
        } else {
          if (!encoding->SetEncryptionID(
                  reinterpret_cast<const uint8*>(
                      webm_crypt.vid_enc.content_id.data()),
                  webm_crypt.vid_enc.content_id.size())) {
            printf("\n Could not set encryption id for video track.\n");
            return -1;
          }
        }
      }
    } else if (track_type == kAudioTrack) {
      // Get the audio track from the parser
      const mkvparser::AudioTrack* const pAudioTrack =
          reinterpret_cast<const mkvparser::AudioTrack*>(parser_track);
      const int64 channels =  pAudioTrack->GetChannels();
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
          reinterpret_cast<mkvmuxer::AudioTrack*>(
              muxer_segment->GetTrackByNumber(aud_track));
      if (!audio) {
        printf("\n Could not get audio track.\n");
        return -1;
      }

      if (track_name)
        audio->set_name(track_name);

      size_t private_size;
      const uint8* const private_data =
          pAudioTrack->GetCodecPrivate(private_size);
      if (private_size > 0) {
        if (!audio->SetCodecPrivate(private_data, private_size)) {
          printf("\n Could not add audio private data.\n");
          return -1;
        }
      }

      const int64 bit_depth = pAudioTrack->GetBitDepth();
      if (bit_depth > 0)
        audio->set_bit_depth(bit_depth);

      if (webm_crypt.audio) {
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

        mkvmuxer::ContentEncAESSettings* const aes =
            encoding->enc_aes_settings();
        if (!aes) {
          printf("\n Error getting audio ContentEncAESSettings.\n");
          return -1;
        }
        if (!aes->SetCipherInitData(
                reinterpret_cast<const uint8*>(
                    webm_crypt.aud_enc.init_data.data()),
                webm_crypt.aud_enc.init_data.length())) {
          printf("\n Could not set Cipher Init Data.\n");
          return -1;
        }

        aud_key.reset(GenerateKeyCheckFile(webm_crypt.aud_enc));
        if (!aud_key.get()) {
          printf("\n Could not generate encryption key.\n");
          return -1;
        }

        // Copy the audio raw key for later.
        if (!webm_crypt.aud_enc.key_file.empty()) {
          string raw_key;
          if (!aud_key->GetRawKey(&raw_key)) {
            printf("Could not get audio raw key.\n");
            return -1;
          }
          aud_raw_key = raw_key;
        }

        if (webm_crypt.aud_enc.content_id.empty()) {
          // If the content id is empty set the content id to the raw key.
          // TODO(fgalligan): This feature may need to be taken out later.
          string raw_key;
          if (!aud_key->GetRawKey(&raw_key)) {
            printf("Could not get raw key to set for content id.\n");
            return -1;
          }

          if (!encoding->SetEncryptionID(
              reinterpret_cast<const uint8*>(raw_key.data()),
              raw_key.size())) {
            printf("\n Could not set encryption id for audio track.\n");
            return -1;
          }
        } else {
          if (!encoding->SetEncryptionID(
                  reinterpret_cast<const uint8*>(
                      webm_crypt.aud_enc.content_id.data()),
                  webm_crypt.aud_enc.content_id.size())) {
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
  scoped_array<uint8> data;
  int data_len = 0;
  EncryptModule audio_encryptor(webm_crypt.aud_enc, aud_key.release());
  EncryptModule video_encryptor(webm_crypt.vid_enc, vid_key.release());
  const mkvparser::Cluster* cluster = parser_segment->GetFirst();

  while ((cluster != NULL) && !cluster->EOS()) {
    const mkvparser::BlockEntry* block_entry;
    int status = cluster->GetFirst(block_entry);
    if (status)
      return -1;

    while ((block_entry != NULL) && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      const int64 trackNum = block->GetTrackNumber();
      const mkvparser::Track* const parser_track =
          parser_tracks->GetTrackByNumber(static_cast<uint32>(trackNum));
      const int64 track_type = parser_track->GetType();

      if ((track_type == kAudioTrack) ||
          (track_type == kVideoTrack)) {
        const int frame_count = block->GetFrameCount();
        const int64 time_ns = block->GetTime(cluster);
        const bool is_key = block->IsKey();

        for (int i = 0; i < frame_count; ++i) {
          const mkvparser::Block::Frame& frame = block->GetFrame(i);

          if (frame.len > data_len) {
            data.reset(new (std::nothrow) uint8[frame.len]);  // NOLINT
            if (!data.get())
              return -1;
            data_len = frame.len;
          }

          if (frame.Read(&reader, data.get()))
            return -1;

          const uint64 track_num =
              (track_type == kAudioTrack) ? aud_track : vid_track;

          if ( (track_type == kVideoTrack && webm_crypt.video) ||
              (track_type == kAudioTrack && webm_crypt.audio) ) {
            string ciphertext;
            if (track_type == kAudioTrack) {
              if (!audio_encryptor.EncryptData(data.get(),
                                               frame.len,
                                               time_ns,
                                               &ciphertext)) {
                printf("\n Could not encrypt audio data.\n");
                return -1;
              }
            } else {
              if (!video_encryptor.EncryptData(data.get(),
                                               frame.len,
                                               time_ns,
                                               &ciphertext)) {
                printf("\n Could not encrypt video data.\n");
                return -1;
              }
            }

            if (!muxer_segment->AddFrame(
                    reinterpret_cast<const uint8*>(ciphertext.data()),
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

      status = cluster->GetNext(block_entry, block_entry);
      if (status)
        return -1;
    }

    cluster = parser_segment->GetNext(cluster);
  }

  muxer_segment->Finalize();

  if (!webm_crypt.aud_enc.key_file.empty()) {
    if (!OutputKeyFile(webm_crypt.aud_enc, "aud_aes.key", aud_raw_key)) {
      printf("Error writing to key file.\n");
      return -1;
    }
  }

  if (!webm_crypt.vid_enc.key_file.empty()) {
    if (!OutputKeyFile(webm_crypt.vid_enc, "vid_aes.key", vid_raw_key)) {
      printf("Error writing to key file.\n");
      return -1;
    }
  }

  writer.Close();
  reader.Close();

  return 0;
}

// Function to decrypt a WebM file. |webm_crypt| encryption settings for
// the source and destination files. Returns 0 on success and <0 for an error.
int WebMDecrypt(const WebMCryptSettings& webm_crypt) {
  scoped_ptr<SymmetricKey> aud_key;

  // If the key_file is empty assume the key is the Content ID.
  if (!webm_crypt.aud_enc.key_file.empty()) {
    aud_key.reset(ReadKeyFromFile(webm_crypt.aud_enc.key_file));
    if (!aud_key.get())
      return -1;
  }

  scoped_ptr<SymmetricKey> vid_key;
  if (!webm_crypt.vid_enc.key_file.empty()) {
    vid_key.reset(ReadKeyFromFile(webm_crypt.vid_enc.key_file));
    if (!vid_key.get())
      return -1;
  }

  mkvparser::MkvReader reader;
  mkvmuxer::MkvWriter writer;
  mkvparser::Segment* parser_segment_ptr;
  mkvmuxer::Segment* muxer_segment_ptr;
  const bool b = OpenWebMFiles(webm_crypt.input,
                               webm_crypt.output,
                               &reader,
                               &parser_segment_ptr,
                               &writer,
                               &muxer_segment_ptr);
  scoped_ptr<mkvparser::Segment> parser_segment(parser_segment_ptr);
  scoped_ptr<mkvmuxer::Segment> muxer_segment(muxer_segment_ptr);
  if (!b) {
    printf("\n Could not open WebM files.\n");
    return -1;
  }

  // Set Tracks element attributes
  enum { kVideoTrack = 1, kAudioTrack = 2 };
  const mkvparser::Tracks* const parser_tracks = parser_segment->GetTracks();
  uint32 i = 0;
  uint64 vid_track = 0;  // no track added
  uint64 aud_track = 0;  // no track added
  EncryptionSettings aud_enc;
  EncryptionSettings vid_enc;
  bool decrypt_video = false;
  bool decrypt_audio = false;

  while (i != parser_tracks->GetTracksCount()) {
    const int track_num = i++;

    const mkvparser::Track* const parser_track =
        parser_tracks->GetTrackByIndex(track_num);

    if (parser_track == NULL)
      continue;

    const char* const track_name = parser_track->GetNameAsUTF8();
    const int64 track_type = parser_track->GetType();

    if (track_type == kVideoTrack) {
      // Get the video track from the parser
      const mkvparser::VideoTrack* const pVideoTrack =
          reinterpret_cast<const mkvparser::VideoTrack*>(parser_track);
      const int64 width =  pVideoTrack->GetWidth();
      const int64 height = pVideoTrack->GetHeight();

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
          reinterpret_cast<mkvmuxer::VideoTrack*>(
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

      // Check if the stream is encrypted.
      const int encoding_count = pVideoTrack->GetContentEncodingCount();
      if (encoding_count > 0) {
        // Only check the first content encoding.
        const ContentEncoding* const encoding =
            pVideoTrack->GetContentEncodingByIndex(0);
        if (!encoding) {
          printf("\n Could not get first ContentEncoding.\n");
          return -1;
        }

        ParseContentEncryption(*encoding, &vid_enc);

        if (vid_key.get() == NULL) {
          if (!webm_crypt.vid_enc.key_password.empty() &&
              !webm_crypt.vid_enc.key_salt.empty()) {
            printf("\n Generating key from password and salt.\n");
            vid_key.reset(SymmetricKey::DeriveKeyFromPassword(
                              SymmetricKey::AES,
                              webm_crypt.vid_enc.key_password,
                              webm_crypt.vid_enc.key_salt,
                              1000,
                              256));
          } else if (webm_crypt.vid_enc.key_file.empty()) {
            // Check to see if there is a content id and assume that is the
            // encryption key.
            // TODO(fgalligan): Remove the code assuming the decryption key is
            // the ContentEncKeyID.
            vid_key.reset(GenerateKeyFromContentID(*encoding));
          }
        }

        decrypt_video = true;
      }

    } else if (track_type == kAudioTrack) {
      // Get the audio track from the parser
      const mkvparser::AudioTrack* const pAudioTrack =
          reinterpret_cast<const mkvparser::AudioTrack*>(parser_track);
      const int64 channels =  pAudioTrack->GetChannels();
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
          reinterpret_cast<mkvmuxer::AudioTrack*>(
              muxer_segment->GetTrackByNumber(aud_track));
      if (!audio) {
        printf("\n Could not get audio track.\n");
        return -1;
      }

      if (track_name)
        audio->set_name(track_name);

      size_t private_size;
      const uint8* const private_data =
          pAudioTrack->GetCodecPrivate(private_size);
      if (private_size > 0) {
        if (!audio->SetCodecPrivate(private_data, private_size)) {
          printf("\n Could not add audio private data.\n");
          return -1;
        }
      }

      const int64 bit_depth = pAudioTrack->GetBitDepth();
      if (bit_depth > 0)
        audio->set_bit_depth(bit_depth);

      // Check if the stream is encrypted.
      const int encoding_count = pAudioTrack->GetContentEncodingCount();
      if (encoding_count > 0) {
        // Only check the first content encoding.
        const ContentEncoding* const encoding =
            pAudioTrack->GetContentEncodingByIndex(0);
        if (!encoding) {
          printf("\n Could not get first ContentEncoding.\n");
          return -1;
        }

        ParseContentEncryption(*encoding, &aud_enc);

        if (aud_key.get() == NULL) {
          if (!webm_crypt.aud_enc.key_password.empty() &&
              !webm_crypt.aud_enc.key_salt.empty()) {
            printf("\n Generating key from password and salt.\n");
            aud_key.reset(SymmetricKey::DeriveKeyFromPassword(
                              SymmetricKey::AES,
                              webm_crypt.aud_enc.key_password,
                              webm_crypt.aud_enc.key_salt,
                              1000,
                              256));
          } else if (webm_crypt.aud_enc.key_file.empty()) {
            // Check to see if there is a content id and assume that is the
            // encryption key.
            // TODO(fgalligan): Remove the code assuming the decryption key is
            // the ContentEncKeyID.
            aud_key.reset(GenerateKeyFromContentID(*encoding));
          }
        }

        decrypt_audio = true;
      }
    }
  }

  // Set Cues element attributes
  muxer_segment->CuesTrack(vid_track);

  // Write clusters
  scoped_array<uint8> data;
  int data_len = 0;
  DecryptModule audio_decryptor(aud_enc, aud_key.release());
  if (decrypt_audio && !audio_decryptor.Init()) {
    printf("\n Could not initialize audio decryptor.\n");
    return -1;
  }

  DecryptModule video_decryptor(vid_enc, vid_key.release());
  if (decrypt_video && !video_decryptor.Init()) {
    printf("\n Could not initialize video decryptor.\n");
    return -1;
  }
  const mkvparser::Cluster* cluster = parser_segment->GetFirst();

  while ((cluster != NULL) && !cluster->EOS()) {
    const mkvparser::BlockEntry* block_entry;
    int status = cluster->GetFirst(block_entry);
    if (status)
      return -1;

    while ((block_entry != NULL) && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      const int64 trackNum = block->GetTrackNumber();
      const mkvparser::Track* const parser_track =
          parser_tracks->GetTrackByNumber(static_cast<uint32>(trackNum));
      const int64 track_type = parser_track->GetType();

      if ((track_type == kAudioTrack) ||
          (track_type == kVideoTrack)) {
        const int frame_count = block->GetFrameCount();
        const int64 time_ns = block->GetTime(cluster);
        const bool is_key = block->IsKey();

        for (int i = 0; i < frame_count; ++i) {
          const mkvparser::Block::Frame& frame = block->GetFrame(i);

          if (frame.len > data_len) {
            data.reset(new (std::nothrow) uint8[frame.len]);  // NOLINT
            if (!data.get())
              return -1;
            data_len = frame.len;
          }

          if (frame.Read(&reader, data.get()))
            return -1;

          const uint64 track_num =
              (track_type == kAudioTrack) ? aud_track : vid_track;

          if ( (track_type == kVideoTrack && decrypt_video) ||
              (track_type == kAudioTrack && decrypt_audio) ) {
            string decrypttext;

            if (track_type == kAudioTrack) {
              if (!audio_decryptor.DecryptData(data.get(),
                                               frame.len,
                                               time_ns,
                                               &decrypttext)) {
                printf("\n Could not decrypt audio data.\n");
                return -1;
              }
            } else {
              if (!video_decryptor.DecryptData(data.get(),
                                               frame.len,
                                               time_ns,
                                               &decrypttext)) {
                printf("\n Could not decrypt video data.\n");
                return -1;
              }
            }

            if (!muxer_segment->AddFrame(
                    reinterpret_cast<const uint8*>(decrypttext.data()),
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

      status = cluster->GetNext(block_entry, block_entry);
      if (status)
        return -1;
    }

    cluster = parser_segment->GetNext(cluster);
  }

  muxer_segment->Finalize();

  writer.Close();
  reader.Close();
  return 0;
}

bool CheckEncryptionOptions(const string& name,
                            const EncryptionSettings& enc) {
  if (enc.cipher_mode != "CTR") {
    printf("stream:%s Only CTR cipher mode supported. mode:%s\n",
           name.c_str(),
           enc.cipher_mode.c_str());
    return false;
  }

  return true;
}

}  // namespace

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
  WebMCryptSettings webm_crypt_settings;
  bool encrypt = true;
  bool test = false;

  // Parse command line options.
  const int argc_check = argc - 1;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp("-h", argv[i]) || !strcmp("-?", argv[i])) {
      Usage();
      return 0;
    } else if (!strcmp("-v", argv[i])) {
      printf("version: %s\n", WEBM_CRYPT_VERSION_STRING);
    } else if (!strcmp("-i", argv[i]) && i < argc_check) {
      webm_crypt_settings.input = argv[++i];
    } else if (!strcmp("-o", argv[i]) && i < argc_check) {
      webm_crypt_settings.output = argv[++i];
    } else if (!strcmp("-audio", argv[i]) && i < argc_check) {
      webm_crypt_settings.audio = !strcmp("true", argv[++i]);
    } else if (!strcmp("-video", argv[i]) && i < argc_check) {
      webm_crypt_settings.video = !strcmp("true", argv[++i]);
    } else if (!strcmp("-decrypt", argv[i])) {
      encrypt = false;
    } else if (!strcmp("-audio_options", argv[i]) && i < argc_check) {
      string option_list(argv[++i]);
      ParseStreamOptions(option_list, &webm_crypt_settings.aud_enc);
    } else if (!strcmp("-video_options", argv[i]) && i < argc_check) {
      string option_list(argv[++i]);
      ParseStreamOptions(option_list, &webm_crypt_settings.vid_enc);
    } else if (!strcmp("-test", argv[i])) {
      test = true;
    }
  }

  if (test) {
    TestEncryption();
    return EXIT_SUCCESS;
  }

  // Check main parameters.
  if (webm_crypt_settings.input.empty()) {
    printf("No input file set.\n");
    Usage();
    return EXIT_FAILURE;
  }
  if (webm_crypt_settings.output.empty()) {
    printf("No output file set.\n");
    Usage();
    return EXIT_FAILURE;
  }

  if (encrypt) {
    if (webm_crypt_settings.audio &&
       !CheckEncryptionOptions("audio", webm_crypt_settings.aud_enc)) {
      Usage();
      return EXIT_FAILURE;
    }
    if (webm_crypt_settings.video &&
       !CheckEncryptionOptions("video", webm_crypt_settings.vid_enc)) {
      Usage();
      return EXIT_FAILURE;
    }

    const int rv = WebMEncrypt(webm_crypt_settings);
    if (rv < 0) {
      printf("Error encrypting WebM file.\n");
      return EXIT_FAILURE;
    }
  } else {
    const int rv = WebMDecrypt(webm_crypt_settings);
    if (rv < 0) {
      printf("Error decrypting WebM file.\n");
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
