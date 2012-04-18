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
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"
#include "mkvmuxer.hpp"
#include "mkvmuxerutil.hpp"
#include "mkvparser.hpp"
#include "mkvreader.hpp"
#include "mkvwriter.hpp"
#include "webm_endian.h"

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
        hmac_key_file(),
        initial_iv(0),
        key_file(),
        key_password(),
        key_salt() {
  }

  // AES encryption algorithm. Currently only "CTR" is supported.
  string cipher_mode;

  // WebM Content ID element.
  string content_id;

  // Path to a file which holds the HMAC key.
  string hmac_key_file;

  // Initial Initialization Vector for encryption.
  uint64 initial_iv;

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
        check_integrity(true),
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

  // Flag telling if the app should check the integrity of the data on decrypt.
  bool check_integrity;

  // Encryption settings for the audio stream.
  EncryptionSettings aud_enc;

  // Encryption settings for the video stream.
  EncryptionSettings vid_enc;
};

// Class to encrypt data for one WebM stream according to the WebM encryption
// RFC specification.
class EncryptModule {
 public:
  static const size_t kHMACKeySize = 20;
  static const size_t kKeySize = 16;
  static const size_t kIntegrityCheckSize = 12;
  static const size_t kSHA1DigestSize = 20;

  // |enc| Encryption settings for a stream. |key| Encryption key for a
  // stream and is owned by this class.
  EncryptModule(const EncryptionSettings& enc,
                SymmetricKey* key,
                const string& hmac_key);
  ~EncryptModule() {}

  // Encrypts |data| according to the encryption settings and encryption key.
  // |length| is the size of |data| in bytes. |ciphertext| is the returned
  // encrypted data. Returns true if |data| was encrypted and passed back
  // through |ciphertext|.
  bool EncryptData(const uint8* data, size_t length, string* ciphertext);

  // Generates a 16 byte CTR Counter Block. The format is
  // | iv | block counter |. |iv| is an 8 byte CTR IV. |counter_block| is an
  // output string containing the Counter Block. Returns true on success.
  static bool GenerateCounterBlock(uint64 iv, string* counter_block);

 private:
  // Encryption settings for the stream.
  const EncryptionSettings enc_;

  // Encryption key for the stream.
  const scoped_ptr<SymmetricKey> key_;

  // HMAC key for the stream.
  const string hmac_key_;

  // The next IV.
  uint64 next_iv_;
};

EncryptModule::EncryptModule(const EncryptionSettings& enc,
                             SymmetricKey* key,
                             const string& hmac_key)
    : enc_(enc),
      key_(key),
      hmac_key_(hmac_key),
      next_iv_(enc.initial_iv) {
}

bool EncryptModule::EncryptData(const uint8* data, size_t length,
                                string* ciphertext) {
  if (!ciphertext)
    return false;

  Encryptor encryptor;
  if (!encryptor.Init(key_.get(), Encryptor::CTR, "")) {
    printf("\n Could not initialize encryptor.\n");
    return false;
  }

  // Set the IV.
  const uint64 iv = next_iv_++;

  string counter_block;
  if (!GenerateCounterBlock(iv, &counter_block)) {
    printf("\n Could not generate counter block.\n");
    return false;
  }

  if (!encryptor.SetCounter(counter_block)) {
    printf("\n Could not set counter.\n");
    return false;
  }

  const string plaintext(reinterpret_cast<const char*>(data), length);
  if (!encryptor.Encrypt(plaintext, ciphertext)) {
    printf("\n Could not encrypt data.\n");
    return false;
  }

  // Prepend the IV.
  const uint64 be_iv = webm_tools::host_to_bigendian(iv);
  char temp[sizeof(be_iv)];
  memcpy(temp, &be_iv, sizeof(be_iv));
  ciphertext->insert(0, temp, sizeof(be_iv));

  crypto::HMAC hmac(crypto::HMAC::SHA1);
  if (!hmac.Init(reinterpret_cast<const uint8*>(hmac_key_.data()),
                 hmac_key_.size())) {
    printf("\n Could not initialize HMAC.\n");
    return false;
  }

  uint8 calculated_hmac[kSHA1DigestSize];
  if (!hmac.Sign(*ciphertext, calculated_hmac, kSHA1DigestSize)) {
    printf("\n Could not calculate HMAC.\n");
    return false;
  }

  // Prepend an integrity check on the IV and encrypted frame.
  const char* const hmac_data = reinterpret_cast<const char*>(calculated_hmac);
  ciphertext->insert(0, hmac_data, kIntegrityCheckSize);
  return true;
}

bool EncryptModule::GenerateCounterBlock(uint64 iv, string* counter_block) {
  if (!counter_block)
    return false;
  char temp[kKeySize];

  // Set the IV.
  memcpy(temp, &iv, sizeof(iv));
  const uint32 offset = sizeof(iv);

  // Set block counter to all 0's.
  memset(temp + offset, 0, kKeySize - offset);

  counter_block->assign(temp, sizeof(temp));
  return true;
}

// Class to decrypt data for one WebM stream according to the WebM encryption
// RFC specification.
class DecryptModule {
 public:
  static const size_t kIntegrityCheckSize = 12;

  DecryptModule(const EncryptionSettings& enc,
                SymmetricKey* key,
                const string& hmac_key);
  ~DecryptModule() {}

  // Initializes and Checks the one time decryption data. Returns true on
  // success.
  bool Init();

  // Decrypts |data| according to the encryption settings and encryption key.
  // |length| is the size of |data| in bytes. |decrypttext| is the returned
  // decrypted data. Returns true if |data| was decrypted and passed back
  // through |decrypttext|.
  bool DecryptData(const uint8* data, size_t length, string* decrypttext);

  void set_check_integrity(bool check) { check_integrity_ = check; }

 private:
  // Encryption settings for the stream.
  const EncryptionSettings enc_;

  // Encryption key for the stream.
  const scoped_ptr<SymmetricKey> key_;

  // Flag to check the integrity of the data.
  bool check_integrity_;

  // HMAC key for the stream.
  const string hmac_key_;

  // Decryption class.
  Encryptor encryptor_;
};

DecryptModule::DecryptModule(const EncryptionSettings& enc,
                             SymmetricKey* key,
                             const string& hmac_key)
    : enc_(enc),
      key_(key),
      check_integrity_(true),
      hmac_key_(hmac_key),
      encryptor_() {
}

bool DecryptModule::Init() {
  if (!key_.get()) {
    printf("\n Could not initialize decryptor. Key was NULL.\n");
    return false;
  }

  if (!encryptor_.Init(key_.get(), Encryptor::CTR, "")) {
    printf("\n Could not initialize decryptor.\n");
    return false;
  }
  return true;
}

bool DecryptModule::DecryptData(const uint8* data, size_t length,
                                string* decrypttext) {
  if (!decrypttext)
    return false;

  if (check_integrity_) {
    crypto::HMAC hmac(crypto::HMAC::SHA1);
    if (!hmac.Init(reinterpret_cast<const uint8*>(hmac_key_.data()),
                   hmac_key_.size())) {
      printf("\n Could not initialize HMAC.\n");
      return false;
    }

    const string check_text(
        reinterpret_cast<const char*>(data + kIntegrityCheckSize),
        length - kIntegrityCheckSize);
    uint8 calculated_hmac[EncryptModule::kSHA1DigestSize];
    if (!hmac.Sign(check_text,
                   calculated_hmac,
                   EncryptModule::kSHA1DigestSize)) {
      printf("\n Could not calculate HMAC.\n");
      return false;
    }

    const string hmac_trunc(reinterpret_cast<const char*>(calculated_hmac),
                            kIntegrityCheckSize);
    const string hmac_data(reinterpret_cast<const char*>(data),
                           kIntegrityCheckSize);
    if (hmac_data.compare(hmac_trunc) != 0) {
      printf("\n Integrity Check Failure. Skipping frame.\n");
      decrypttext->clear();
      return true;
    }
  }

  uint64 iv;
  memcpy(&iv, data + kIntegrityCheckSize, sizeof(iv));
  iv = webm_tools::bigendian_to_host(iv);

  string counter_block;
  if (!EncryptModule::GenerateCounterBlock(iv, &counter_block)) {
    printf("\n Could not generate counter block.\n");
    return false;
  }

  if (!encryptor_.SetCounter(counter_block)) {
    printf("\n Could not set counter.\n");
    return false;
  }

  // Skip past the integrity check and IV.
  const size_t offset = kIntegrityCheckSize + sizeof(iv);
  const string encryptedtext(
      reinterpret_cast<const char*>(data + offset),
      length - offset);
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
  printf("  -check_integrity     Check Integrity of data. (Default true)\n");
  printf("  \n");
  printf("-audio_options <string> Semicolon separated name value pair.\n");
  printf("  key_file=<string>     Path to key file. (Default empty)\n");
  printf("  content_id=<string>   Encryption content ID. (Default empty)\n");
  printf("  key_password=<string> Password used to generate a key. (\n");
  printf("                        Default empty)\n");
  printf("  key_salt=<string>     Salt used to generate a key. (Default\n");
  printf("                        empty)\n");
  printf("  initial_iv=<uint64>   Initial IV value. (Default random)\n");
  printf("  hmac_file=<string>    Path to HMAC key file. (Default empty)\n");
  printf("  \n");
  printf("-video_options <string> Semicolon separated name value pair.\n");
  printf("  key_file=<string>     Path to key file. (Default empty)\n");
  printf("  content_id=<string>   Encryption content ID. (Default empty)\n");
  printf("  key_password=<string> Password used to generate a key. (\n");
  printf("                        Default empty)\n");
  printf("  key_salt=<string>     Salt used to generate a key. (Default\n");
  printf("                        empty)\n");
  printf("  initial_iv=<uint64>   Initial IV value. (Default random)\n");
  printf("  hmac_file=<string>    Path to HMAC key file. (Default empty)\n");
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
    raw_data[i] = static_cast<uint8>(i);
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
// reader class output parameter. |parser| WebM parser class output parameter.
// |writer| WebM writer class output parameter. |muxer| WebM muxer class
// output parameter.
bool OpenWebMFiles(const string& input,
                   const string& output,
                   mkvparser::MkvReader* reader,
                   scoped_ptr<mkvparser::Segment>* parser,
                   mkvmuxer::MkvWriter* writer,
                   scoped_ptr<mkvmuxer::Segment>* muxer) {
  if (!reader || !parser || !writer || !muxer)
    return false;

  if (reader->Open(input.c_str())) {
    printf("\n Filename is invalid or error while opening.\n");
    return false;
  }

  long long pos = 0;  // NOLINT
  mkvparser::EBMLHeader ebml_header;
  if (ebml_header.Parse(reader, pos)) {
    printf("\n File:%s is not WebM file.", input.c_str());
    return false;
  }

  mkvparser::Segment* parser_segment;
  int64 ret = mkvparser::Segment::CreateInstance(reader, pos, parser_segment);
  if (ret) {
    printf("\n Segment::CreateInstance() failed.");
    return false;
  }
  parser->reset(parser_segment);

  ret = (*parser)->Load();
  if (ret < 0) {
    printf("\n Segment::Load() failed.");
    return false;
  }

  const mkvparser::SegmentInfo* const segment_info = (*parser)->GetInfo();
  const int64 timeCodeScale = segment_info->GetTimeCodeScale();

  // Set muxer header info
  if (!writer->Open(output.c_str())) {
    printf("\n Filename is invalid or error while opening.\n");
    return false;
  }

  muxer->reset(new (std::nothrow) mkvmuxer::Segment());  // NOLINT
  if (!(*muxer)->Init(writer)) {
    printf("\n Could not initialize muxer segment!\n");
    return false;
  }
  (*muxer)->set_mode(mkvmuxer::Segment::kFile);

  // Set SegmentInfo element attributes
  mkvmuxer::SegmentInfo* const info = (*muxer)->GetSegmentInfo();
  info->set_timecode_scale(timeCodeScale);
  string app_str("webm_crypt ");
  app_str += WEBM_CRYPT_VERSION_STRING;
  info->set_writing_app(app_str.c_str());
  return true;
}

// Reads data from |file| and sets it to |data|. Returns true on success.
bool ReadDataFromFile(const string& file, string* data) {
  if (!data || file.empty())
    return false;

  FILE* const f = fopen(file.c_str(), "rb");
  if (!f)
    return false;

  fseek(f, 0, SEEK_END);
  const int key_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  scoped_array<char> raw_key_buf(new (std::nothrow) char[key_size]);  // NOLINT
  const int bytes_read = fread(raw_key_buf.get(), 1, key_size, f);
  fclose(f);

  if (bytes_read != key_size)
    return false;

  data->assign(raw_key_buf.get(), bytes_read);
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

      scoped_array<char> raw_key_buf(
          new (std::nothrow) char[key_size]);  // NOLINT
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

// Outputs a key to a file. |key_file| path to a file. May be empty.
// |default_name| path to a file if the |key_file| was not set. |raw_key|
// encryption key. Returns true on success.
bool OutputKeyFile(const string& key_file,
                   const string& default_name,
                   const string& raw_key) {
  string key_output_file;
  if (!key_file.empty()) {
    FILE* const f = fopen(key_file.c_str(), "rb");
    if (f) {
      // Key file was passed in.
      fclose(f);
    } else {
      key_output_file = key_file;
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

double StringToDouble(const string& s) {
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
      } else if (name == "initial_iv") {
        enc->initial_iv = strtoull(value.c_str(), NULL, 10);
      } else if (name == "hmac_file") {
        enc->hmac_key_file = value;
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
  scoped_ptr<mkvparser::Segment> parser_segment;
  scoped_ptr<mkvmuxer::Segment> muxer_segment;
  const bool b = OpenWebMFiles(webm_crypt.input,
                               webm_crypt.output,
                               &reader,
                               &parser_segment,
                               &writer,
                               &muxer_segment);
  if (!b) {
    printf("\n Could not open WebM files.\n");
    return -1;
  }

  // Set Tracks element attributes
  const mkvparser::Tracks* const parser_tracks = parser_segment->GetTracks();
  uint32 i = 0;
  uint64 vid_track = 0;  // no track added
  uint64 aud_track = 0;  // no track added
  string aud_raw_key;
  string vid_raw_key;
  string hmac_aud_raw_key;
  string hmac_vid_raw_key;

  while (i != parser_tracks->GetTracksCount()) {
    const int track_num = i++;

    const mkvparser::Track* const parser_track =
        parser_tracks->GetTrackByIndex(track_num);

    if (parser_track == NULL)
      continue;

    const char* const track_name = parser_track->GetNameAsUTF8();
    const int64 track_type = parser_track->GetType();

    if (track_type == mkvparser::Track::kVideo) {
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
        if (aes->cipher_mode() != mkvmuxer::ContentEncAESSettings::kCTR) {
          printf("\n Cipher Mode is not CTR.\n");
          return -1;
        }

        vid_key.reset(GenerateKeyCheckFile(webm_crypt.vid_enc));
        if (!vid_key.get()) {
          printf("\n Could not generate encryption key.\n");
          return -1;
        }

        // Check if the HMAC key is in a file.
        if (!webm_crypt.vid_enc.hmac_key_file.empty()) {
          if (!ReadDataFromFile(webm_crypt.vid_enc.hmac_key_file,
                                &hmac_vid_raw_key)) {
            printf("\n Could not read HMAC key from file:%s\n",
                   webm_crypt.vid_enc.hmac_key_file.c_str());
            return -1;
          }
          if (hmac_vid_raw_key.size() != EncryptModule::kHMACKeySize) {
             printf("\n HMAC key from file:%s is not %zd bytes.\n",
                    webm_crypt.vid_enc.hmac_key_file.c_str(),
                    EncryptModule::kHMACKeySize);
            return -1;
          }
        } else if (hmac_vid_raw_key.empty()) {
          for (uint32 i = 0; i < EncryptModule::kHMACKeySize; ++i) {
            hmac_vid_raw_key.push_back(static_cast<char>(rand()));
          }
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
          // If the content id is empty, create a content id.
          const uint64 enc_id = rand() | static_cast<uint64>(rand()) << 32;
          if (!encoding->SetEncryptionID(
              reinterpret_cast<const uint8*>(&enc_id),
              sizeof(enc_id))) {
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
    } else if (track_type == mkvparser::Track::kAudio) {
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
        if (aes->cipher_mode() != mkvmuxer::ContentEncAESSettings::kCTR) {
          printf("\n Cipher Mode is not CTR.\n");
          return -1;
        }

        // Check if the HMAC key is in a file.
        if (!webm_crypt.aud_enc.hmac_key_file.empty()) {
          if (!ReadDataFromFile(webm_crypt.aud_enc.hmac_key_file,
                                &hmac_aud_raw_key)) {
            printf("\n Could not read HMAC key from file:%s\n",
                   webm_crypt.aud_enc.hmac_key_file.c_str());
            return -1;
          }
          if (hmac_aud_raw_key.size() != EncryptModule::kHMACKeySize) {
             printf("\n HMAC key from file:%s is not %zd bytes.\n",
                    webm_crypt.aud_enc.hmac_key_file.c_str(),
                    EncryptModule::kHMACKeySize);
            return -1;
          }
        } else if (hmac_aud_raw_key.empty()) {
          for (uint32 i = 0; i < EncryptModule::kHMACKeySize; ++i) {
            hmac_aud_raw_key.push_back(static_cast<char>(rand()));
          }
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
          // If the content id is empty, create a content id.
          const uint64 enc_id = rand() | static_cast<uint64>(rand()) << 32;
          if (!encoding->SetEncryptionID(
              reinterpret_cast<const uint8*>(&enc_id),
              sizeof(enc_id))) {
            printf("\n Could not set encryption id for video track.\n");
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
  EncryptModule audio_encryptor(webm_crypt.aud_enc,
                                aud_key.release(),
                                hmac_aud_raw_key);
  EncryptModule video_encryptor(webm_crypt.vid_enc,
                                vid_key.release(),
                                hmac_vid_raw_key);
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

      if ((track_type == mkvparser::Track::kAudio) ||
          (track_type == mkvparser::Track::kVideo)) {
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
              (track_type == mkvparser::Track::kAudio) ? aud_track : vid_track;

          if ((track_type == mkvparser::Track::kVideo && webm_crypt.video) ||
              (track_type == mkvparser::Track::kAudio && webm_crypt.audio) ) {
            string ciphertext;
            if (track_type == mkvparser::Track::kAudio) {
              if (!audio_encryptor.EncryptData(data.get(),
                                               frame.len,
                                               &ciphertext)) {
                printf("\n Could not encrypt audio data.\n");
                return -1;
              }
            } else {
              if (!video_encryptor.EncryptData(data.get(),
                                               frame.len,
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

  // Output encryption keys.
  if (webm_crypt.aud_enc.key_password.empty() ||
      webm_crypt.aud_enc.key_salt.empty()) {
    if (!OutputKeyFile(webm_crypt.aud_enc.key_file,
                       "aud_encryption_aes.key",
                       aud_raw_key)) {
      printf("Error writing to key file.\n");
      return -1;
    }
  }
  if (webm_crypt.vid_enc.key_password.empty() ||
      webm_crypt.vid_enc.key_salt.empty()) {
    if (!OutputKeyFile(webm_crypt.vid_enc.key_file,
                       "vid_encryption_aes.key",
                       vid_raw_key)) {
      printf("Error writing to key file.\n");
      return -1;
    }
  }

  // Output HMAC keys.
  if (!OutputKeyFile(webm_crypt.aud_enc.hmac_key_file,
                     "aud_hmac.key",
                     hmac_aud_raw_key)) {
    printf("Error writing audio HMAC key to file.\n");
    return -1;
  }
  if (!OutputKeyFile(webm_crypt.vid_enc.hmac_key_file,
                     "vid_hmac.key",
                     hmac_vid_raw_key)) {
    printf("Error writing video HMAC key to file.\n");
    return -1;
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

  string hmac_aud_raw_key;
  if (!webm_crypt.aud_enc.hmac_key_file.empty()) {
    if (!ReadDataFromFile(webm_crypt.aud_enc.hmac_key_file, &hmac_aud_raw_key))
      return -1;
  }

  string hmac_vid_raw_key;
  if (!webm_crypt.vid_enc.hmac_key_file.empty()) {
    if (!ReadDataFromFile(webm_crypt.vid_enc.hmac_key_file, &hmac_vid_raw_key))
      return -1;
  }

  mkvparser::MkvReader reader;
  mkvmuxer::MkvWriter writer;
  scoped_ptr<mkvparser::Segment> parser_segment;
  scoped_ptr<mkvmuxer::Segment> muxer_segment;
  const bool b = OpenWebMFiles(webm_crypt.input,
                               webm_crypt.output,
                               &reader,
                               &parser_segment,
                               &writer,
                               &muxer_segment);
  if (!b) {
    printf("\n Could not open WebM files.\n");
    return -1;
  }

  // Set Tracks element attributes
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

    if (track_type == mkvparser::Track::kVideo) {
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
        if (webm_crypt.check_integrity && hmac_vid_raw_key.empty()) {
          printf("\n Cannot check integrity of video. HMAC key is empty.\n");
          return -1;
        }

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
          }
        }

        decrypt_video = true;
      }

    } else if (track_type == mkvparser::Track::kAudio) {
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
        if (webm_crypt.check_integrity && hmac_aud_raw_key.empty()) {
          printf("\n Cannot check integrity of audio. HMAC key is empty.\n");
          return -1;
        }

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
  DecryptModule audio_decryptor(aud_enc, aud_key.release(), hmac_aud_raw_key);
  if (decrypt_audio && !audio_decryptor.Init()) {
    printf("\n Could not initialize audio decryptor.\n");
    return -1;
  }
  audio_decryptor.set_check_integrity(webm_crypt.check_integrity);

  DecryptModule video_decryptor(vid_enc, vid_key.release(), hmac_vid_raw_key);
  if (decrypt_video && !video_decryptor.Init()) {
    printf("\n Could not initialize video decryptor.\n");
    return -1;
  }
  video_decryptor.set_check_integrity(webm_crypt.check_integrity);

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

      if ((track_type == mkvparser::Track::kAudio) ||
          (track_type == mkvparser::Track::kVideo)) {
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
              (track_type == mkvparser::Track::kAudio) ? aud_track : vid_track;

          if ((track_type == mkvparser::Track::kVideo && decrypt_video) ||
              (track_type == mkvparser::Track::kAudio && decrypt_audio) ) {
            string decrypttext;
            if (track_type == mkvparser::Track::kAudio) {
              if (!audio_decryptor.DecryptData(data.get(),
                                               frame.len,
                                               &decrypttext)) {
                printf("\n Could not decrypt audio data.\n");
                return -1;
              }
            } else {
              if (!video_decryptor.DecryptData(data.get(),
                                               frame.len,
                                               &decrypttext)) {
                printf("\n Could not decrypt video data.\n");
                return -1;
              }
            }

            // DecryptData may return true wihtout decrypting data if the
            // integrity check failed.
            if (!decrypttext.empty()) {
              if (!muxer_segment->AddFrame(
                      reinterpret_cast<const uint8*>(decrypttext.data()),
                      decrypttext.size(),
                      track_num,
                      time_ns,
                      is_key)) {
                printf("\n Could not add encrypted frame.\n");
                return -1;
              }
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

  // Create inital random IV values.
  srand(static_cast<uint32>(time(NULL)));
  webm_crypt_settings.aud_enc.initial_iv = rand() |
                                           static_cast<uint64>(rand()) << 32;
  webm_crypt_settings.vid_enc.initial_iv = rand() |
                                           static_cast<uint64>(rand()) << 32;

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
    } else if (!strcmp("-check_integrity", argv[i]) && i < argc_check) {
      webm_crypt_settings.check_integrity = !strcmp("true", argv[++i]);
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
