// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdint.h>
#include <string>

#include "aes_ctr.h"
#include "mkvmuxer.hpp"
#include "mkvmuxerutil.hpp"
#include "mkvparser.hpp"
#include "mkvreader.hpp"
#include "mkvwriter.hpp"
#include "webm_constants.h"
#include "webm_endian.h"

// This application uses the webm library from the libwebm project. See the
// readme.txt for build instructions.

namespace {

using mkvparser::ContentEncoding;
using std::string;
using std::unique_ptr;

const char WEBM_CRYPT_VERSION_STRING[] = "0.3.1.0";

// Struct to hold encryption settings for a single WebM stream.
struct EncryptionSettings {
  EncryptionSettings()
      : base_secret_file(),
        cipher_mode("CTR"),
        content_id(),
        initial_iv(0),
        unencrypted_range(0) {
  }

  // Path to a file which holds the base secret.
  string base_secret_file;

  // AES encryption algorithm. Currently only "CTR" is supported.
  string cipher_mode;

  // WebM Content ID element.
  string content_id;

  // Initial Initialization Vector for encryption.
  uint64_t initial_iv;

  // Do not encrypt frames that have a start time less than
  // |unencrypted_range| in milliseconds.
  int64_t unencrypted_range;
};

// Struct to hold file wide encryption settings.
struct WebMCryptSettings {
  WebMCryptSettings()
      : input(),
        output(),
        video(true),
        audio(false),
        no_encryption(false),
        match_src_clusters(false),
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

  // Flag telling if the app should not encrypt or decrypt the data. This
  // should only be used to test the other parts of the system.
  bool no_encryption;

  // Flag telling app to match the placement of the source WebM Clusters.
  bool match_src_clusters;

  // Encryption settings for the audio stream.
  EncryptionSettings aud_enc;

  // Encryption settings for the video stream.
  EncryptionSettings vid_enc;
};

// Class to encrypt data for one WebM stream according to the WebM encryption
// RFC specification.
// http://wiki.webmproject.org/encryption/webm-encryption-rfc
class EncryptModule {
 public:
  static const size_t kDefaultContentIDSize = 16;
  static const size_t kIVSize = 8;
  static const size_t kKeySize = 16;
  static const size_t kSHA1DigestSize = 20;
  static const size_t kSignalByteSize = 1;
  static const uint8_t kEncryptedFrame = 0x1;

  // |enc| Encryption settings for a stream. |secret| Encryption key for a
  // stream.
  EncryptModule(const EncryptionSettings& enc, const string& secret);
  ~EncryptModule() {}

  // Initializes encryption key. Returns true on success.
  bool Init();

  // Processes |source| according to the encryption settings,
  // |encrypt_frame|, and |key_|. |length| is the size of |source| in bytes.
  // |encrypt_frame| tells the encryptor whether to encrypt the frame or just
  // add a signal byte to the unencrypted frame. |destination| is the returned
  // encrypted data if |encrypt_frame| is true and signal byte + |source|
  // if |encrypt_frame| is false. |destination_size| is the size of |destination|
  // in bytes. Returns true if |source| was processed and passed back
  // through |destination|.
  bool ProcessData(const uint8_t* source, size_t size,
                   bool encrypt_frame,
                   uint8_t* destination, size_t* destination_size);

  void set_do_not_encrypt(bool flag) { do_not_encrypt_ = flag; }

  // Generates a 16 byte CTR Counter Block. The format is
  // | iv | block counter |. |iv| is an 8 byte CTR IV. |counter_block| is an
  // output string containing the Counter Block. Returns true on success.
  static bool GenerateCounterBlock(const string& iv, string* counter_block);

 private:
  // Flag telling if the class should not encrypt the data. This should
  // only be used for testing.
  bool do_not_encrypt_;

  // Encryption settings for the stream.
  const EncryptionSettings enc_;

  // Encryption key for the stream.
  string key_;

  // The next IV.
  uint64_t next_iv_;
};

EncryptModule::EncryptModule(const EncryptionSettings& enc,
                             const string& secret)
    : do_not_encrypt_(false),
      enc_(enc),
      next_iv_(enc.initial_iv) {
  key_.assign(secret);
}

bool EncryptModule::Init() {
  if (key_.size() == 0) {
    fprintf(stderr, "Error creating encryption key.\n");
    return false;
  }
  return true;
}

bool EncryptModule::ProcessData(const uint8_t* source, size_t size,
                                bool encrypt_frame,
                                uint8_t* destination,
                                size_t* destination_size) {
  if (!source || size <= 0)
    return false;

  const bool encrypt_the_frame = do_not_encrypt_ ? false : encrypt_frame;
  unique_ptr<uint8_t[]> cipher_temp;
  size_t cipher_temp_size = size + kSignalByteSize;

  if (encrypt_the_frame) {
    AesCtr128Encryptor encryptor;
    if (!encryptor.InitKey(key_)) {
      fprintf(stderr, "Could not initialize encryptor.\n");
      return false;
    }

    // Set the IV.
    const uint64_t iv = next_iv_++;
    const string iv_str(reinterpret_cast<const char*>(&iv), sizeof(iv));
    string counter_block;
    if (!GenerateCounterBlock(iv_str, &counter_block)) {
      fprintf(stderr, "Could not generate counter block.\n");
      return false;
    }

    if (!encryptor.SetCounter(counter_block)) {
      fprintf(stderr, "Could not set counter.\n");
      return false;
    }

    // Prepend the IV.
    cipher_temp_size += sizeof(iv);
    cipher_temp.reset(new (std::nothrow) uint8_t[cipher_temp_size]);  // NOLINT
    if (!cipher_temp.get())
      return false;

    memcpy(cipher_temp.get() + kSignalByteSize, &iv, sizeof(iv));
    if (!encryptor.Encrypt(source, size,
                           cipher_temp.get() + sizeof(iv) + kSignalByteSize)) {
      fprintf(stderr, "Could not encrypt data.\n");
      return false;
    }
  } else {
    cipher_temp.reset(new (std::nothrow) uint8_t[cipher_temp_size]);  // NOLINT
    if (!cipher_temp.get())
      return false;

    memcpy(cipher_temp.get() + kSignalByteSize, source, size);
  }

  const uint8_t signal_byte = encrypt_the_frame ? kEncryptedFrame : 0;
  cipher_temp[0] = signal_byte;
  memcpy(destination, cipher_temp.get(), cipher_temp_size);
  *destination_size = cipher_temp_size;
  return true;
}

bool EncryptModule::GenerateCounterBlock(const string& iv,
                                         string* counter_block) {
  if (!counter_block || iv.size() != kIVSize)
    return false;

  counter_block->reserve(kKeySize);
  counter_block->append(iv);
  counter_block->append(kKeySize - kIVSize, 0);

  return true;
}

// Class to decrypt data for one WebM stream according to the WebM encryption
// RFC specification.
class DecryptModule {
 public:
  // TODO(fgalligan) Remove the no_decrypt parameter.
  // |enc| Encryption settings for a stream. |secret| Decryption key for a
  // stream. |no_decrypt| If true do not decrypt the stream.
  DecryptModule(const EncryptionSettings& enc,
                const string& secret,
                bool no_decrypt);
  ~DecryptModule() {}

  // Initializes and Checks the one time decryption data. Returns true on
  // success.
  bool Init();

  // Decrypts |source| according to the encryption settings and encryption key.
  // |length| is the size of |source| in bytes. |destination| is the returned
  // decrypted data if |source| was decrypted. If data was unencrypted then
  // |destination| is the original data. Returns true if |data| was decrypted
  // and passed back through |destination|.
  bool DecryptData(const uint8_t* data, size_t length, uint8_t* destination,
                   size_t *destination_size);

 private:
  // Flag telling if the class should not decrypt the data. This should
  // only be used for testing.
  const bool do_not_decrypt_;

  // Encryption settings for the stream.
  const EncryptionSettings enc_;

  // Encryption key for the stream.
  string key_;

  // Decryption class.
  AesCtr128Encryptor encryptor_;
};

DecryptModule::DecryptModule(const EncryptionSettings& enc,
                             const string& secret,
                             bool no_decrypt)
    : do_not_decrypt_(no_decrypt),
      enc_(enc) {
  key_.assign(secret);
}

bool DecryptModule::Init() {
  if (key_.size() == 0) {
    fprintf(stderr, "Error creating encryption key.\n");
    return false;
  }

  if (!do_not_decrypt_) {
    if (!encryptor_.InitKey(key_)) {
      fprintf(stderr, "Could not initialize decryptor.\n");
      return false;
    }
  }
  return true;
}

bool DecryptModule::DecryptData(const uint8_t* source, size_t length,
                                uint8_t* destination, size_t *destination_size) {
  if (!source || length <= 0)
    return false;

  size_t offset = 0;

  if (!do_not_decrypt_) {
    if (length == 0) {
      fprintf(stderr, "Length of encrypted data is 0.\n");
      return false;
    }
    const uint8_t signal_byte = source[0];

    if (signal_byte & EncryptModule::kEncryptedFrame) {
      if (length < EncryptModule::kSignalByteSize + EncryptModule::kIVSize) {
        fprintf(stderr, "Not enough data to read IV.\n");
        return false;
      }

      const char* iv_data =
          reinterpret_cast<const char*>(source + EncryptModule::kSignalByteSize);
      const string iv(iv_data, EncryptModule::kIVSize);

      string counter_block;
      if (!EncryptModule::GenerateCounterBlock(iv, &counter_block)) {
        fprintf(stderr, "Could not generate counter block.\n");
        return false;
      }

      if (!encryptor_.SetCounter(counter_block)) {
        fprintf(stderr, "Could not set counter.\n");
        return false;
      }

      offset = EncryptModule::kSignalByteSize + EncryptModule::kIVSize;

      if (!encryptor_.Encrypt(source + offset, length - offset, destination)) {
        fprintf(stderr, "Could not decrypt data.\n");
        return false;
      }
    } else {
      offset = EncryptModule::kSignalByteSize;
      memcpy(destination, source + offset, length - offset);
    }
  } else {
    offset = EncryptModule::kSignalByteSize;
    memcpy(destination, source + offset, length - offset);
  }

  *destination_size = length - offset;
  return true;
}

void Usage() {
  printf("Usage: webm_crypt [-test] -i <input> -o <output> [Main options] "
         "[audio options] [video options]\n");
  printf("\n");
  printf("Main options:\n");
  printf("  -h | -?               Show help.\n");
  printf("  -v                    Show version\n");
  printf("  -test                 Tests the encryption and decryption.\n");
  printf("  -audio <bool>         Process audio stream. (Default false)\n");
  printf("  -video <bool>         Process video stream. (Default true)\n");
  printf("  -decrypt              Decrypt the stream. (Default encrypt)\n");
  printf("  -no_encryption        Test flag which will not encrypt or\n");
  printf("                        decrypt the data. (Default false)\n");
  printf("  -match_src_clusters   Flag to match source WebM (Default false)\n");
  printf("  \n");
  printf("-audio_options <string> Comma separated name value pair.\n");
  printf("  content_id=<string>   Encryption content ID. (Default empty)\n");
  printf("  initial_iv=<uint64_t>   Initial IV value. (Default random)\n");
  printf("  base_file=<string>    Path to base secret file. (Default\n");
  printf("                        empty)\n");
  printf("  unencrypted_range=<int64> Do not encrypt frames from\n");
  printf("                        [0, value) milliseconds (Default value=0)\n");
  printf("  \n");
  printf("-video_options <string> Comma separated name value pair.\n");
  printf("  content_id=<string>   Encryption content ID. (Default empty)\n");
  printf("  initial_iv=<uint64_t>   Initial IV value. (Default random)\n");
  printf("  base_file=<string>    Path to base secret file. (Default\n");
  printf("                        empty)\n");
  printf("  unencrypted_range=<int64> Do not encrypt frames from\n");
  printf("                        [0, value) milliseconds (Default value=0)\n");
}

void TestEncryption() {
  using std::cout;
  using std::endl;

  unsigned char enc_key[AES_BLOCK_SIZE] = { 0 };
  const string kInitialCounter(16, '0');
  AesCtr128Encryptor encryptor;

  // Generate a random key.
  if (!RAND_bytes(enc_key, AES_BLOCK_SIZE)) {
    cout << "Fail to generate a random encode key" << endl;
    return;
  }

  string key_string(reinterpret_cast<const char*>(enc_key));
  bool b = encryptor.InitKey(key_string);
  if (!b) {
    fprintf(stderr, "Could not initialize encrypt object.\n");
    return;
  }

  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    fprintf(stderr, "Could not SetCounter on encryptor.\n");
    return;
  }

  string plaintext("this is the plaintext");
  size_t size = plaintext.size();
  unique_ptr<uint8_t[]> out(new uint8_t[size]);

  b = encryptor.Encrypt(reinterpret_cast<const uint8_t*>(plaintext.c_str()),
                        size, out.get());
  if (!b) {
    fprintf(stderr, "Could not encrypt data.\n");
    return;
  }
  string ciphertext(out.get(), out.get() + size);

  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    fprintf(stderr, "Could not SetCounter on encryptor.\n");
    return;
  }

  b = encryptor.Encrypt(reinterpret_cast<const uint8_t*>(ciphertext.c_str()),
                        size, out.get());
  if (!b) {
    fprintf(stderr, "Could not decrypt data.\n");
    return;
  }

  string decrypted(out.get(), out.get() + size);

  printf("Test 1 finished.\n");
  cout << "iv         :" << kInitialCounter << endl;
  cout << "raw_key    :" << enc_key << endl;
  cout << "plaintext  :" << plaintext << endl;
  cout << "ciphertext :" << ciphertext << endl;
  cout << "decrypted  :" << decrypted << endl;

  const int kNonAsciiSize = 256;
  uint8_t raw_data[kNonAsciiSize];
  for (int i = 0; i < kNonAsciiSize; ++i) {
    raw_data[i] = static_cast<uint8_t>(i);
  }
  string non_ascii(reinterpret_cast<char*>(raw_data), kNonAsciiSize);

  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    fprintf(stderr, "Could not SetCounter on encryptor.\n");
    return;
  }

  out.reset(new (std::nothrow) uint8_t[kNonAsciiSize]);  // NOLINT
  b = encryptor.Encrypt(raw_data, kNonAsciiSize, out.get());
  if (!b) {
    fprintf(stderr, "Could not encrypt data.\n");
    return;
  }
  ciphertext = string(out.get(), out.get() + kNonAsciiSize);

  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    fprintf(stderr, "Could not SetCounter on encryptor.\n");
    return;
  }

  b = encryptor.Encrypt(reinterpret_cast<const uint8_t*>(ciphertext.c_str()),
                        kNonAsciiSize, out.get());
  if (!b) {
    fprintf(stderr, "Could not decrypt data.\n");
    return;
  }
  decrypted = string(out.get(), out.get() + kNonAsciiSize);

  printf("Test 2 finished.\n");
  cout << "iv         :" << kInitialCounter << endl;
  cout << "non_ascii  :" << non_ascii << endl;
  cout << "ciphertext :" << ciphertext << endl;
  cout << "decrypted  :" << decrypted << endl;

  non_ascii.append(plaintext);

  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    fprintf(stderr, "Could not SetCounter on encryptor.\n");
    return;
  }

  size = non_ascii.size();
  out.reset(new (std::nothrow) uint8_t[size]);  // NOLINT
  b = encryptor.Encrypt(reinterpret_cast<const uint8_t*>(non_ascii.c_str()),
                        size, out.get());
  if (!b) {
    fprintf(stderr, "Could not encrypt data.\n");
    return;
  }
  ciphertext = string(out.get(), out.get() + size);


  b = encryptor.SetCounter(kInitialCounter);
  if (!b) {
    fprintf(stderr, "Could not SetCounter on encryptor.\n");
    return;
  }

  b = encryptor.Encrypt(reinterpret_cast<const uint8_t*>(ciphertext.c_str()),
                        size, out.get());
  if (!b) {
    fprintf(stderr, "Could not decrypt data.\n");
    return;
  }
  decrypted = string(out.get(), out.get() + size);

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
                   unique_ptr<mkvparser::Segment>* parser,
                   mkvmuxer::MkvWriter* writer,
                   unique_ptr<mkvmuxer::Segment>* muxer) {
  if (!reader || !parser || !writer || !muxer)
    return false;

  if (reader->Open(input.c_str())) {
    fprintf(stderr, "Filename is invalid or error while opening.\n");
    return false;
  }

  long long pos = 0;  // NOLINT
  mkvparser::EBMLHeader ebml_header;
  if (ebml_header.Parse(reader, pos)) {
    fprintf(stderr, "File:%s is not WebM file.", input.c_str());
    return false;
  }

  mkvparser::Segment* parser_segment;
  int64_t ret = mkvparser::Segment::CreateInstance(reader, pos, parser_segment);
  if (ret) {
    fprintf(stderr, "Segment::CreateInstance() failed.");
    return false;
  }
  parser->reset(parser_segment);

  ret = (*parser)->Load();
  if (ret < 0) {
    fprintf(stderr, "Segment::Load() failed.");
    return false;
  }

  const mkvparser::SegmentInfo* const segment_info = (*parser)->GetInfo();
  const int64_t timeCodeScale = segment_info->GetTimeCodeScale();

  // Set muxer header info
  if (!writer->Open(output.c_str())) {
    fprintf(stderr, "Filename is invalid or error while opening.\n");
    return false;
  }

  muxer->reset(new (std::nothrow) mkvmuxer::Segment());  // NOLINT
  if (!(*muxer)->Init(writer)) {
    fprintf(stderr, "Could not initialize muxer segment!\n");
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

  unique_ptr<char[]> raw_key_buf(new (std::nothrow) char[key_size]);  // NOLINT
  const int bytes_read = fread(raw_key_buf.get(), 1, key_size, f);
  fclose(f);

  if (bytes_read != key_size)
    return false;

  data->assign(raw_key_buf.get(), bytes_read);
  return true;
}

// Generate random data of |length| bytes. |data| output string that contains
// the data. Returns true on success.
bool GenerateRandomData(size_t length, string* data) {
  if (!data)
    return false;

  unique_ptr<unsigned char[]> temp(new unsigned char[length]);
  if (!RAND_bytes(temp.get(), length)) {
    fprintf(stderr, "Error generating base secret data.\n");
    return false;
  }

  string newString(temp.get(), temp.get() + length);
  data->assign(newString);
  return true;
}

// Generate random value for a uint64_t. |value| output uint64_t that contains
// the data. Returns true on success.
bool GenerateRandomuint64_t(uint64_t* value) {
  if (!value)
    return false;

  unsigned char* temp = reinterpret_cast<unsigned char*>(value);
  if (!RAND_bytes(temp, sizeof(uint64_t))) {
    fprintf(stderr, "Error generating base secret data.\n");
    return false;
  }

  return true;
}

// Reads the secret data from a file. If a secret file was not set the
// function will generate the secret data. |secret| output string that
// contains the secret data. Returns true on success.
bool GetBaseSecret(const EncryptionSettings& enc, string* secret) {
  if (!secret)
    return false;

  if (!enc.base_secret_file.empty()) {
    // Check if there is a file on disk and if it contains any data.
    FILE* const f = fopen(enc.base_secret_file.c_str(), "rb");
    if (f) {
      fseek(f, 0, SEEK_END);
      const int data_size = ftell(f);
      fseek(f, 0, SEEK_SET);
      fclose(f);

      if (data_size > 0) {
        string data;
        if (!ReadDataFromFile(enc.base_secret_file, &data)) {
          fprintf(stderr, "Could not read base secret file:%s\n",
                  enc.base_secret_file.c_str());
          return false;
        }

        if (data.length() != EncryptModule::kKeySize) {
          fprintf(stderr, "Base secret file data != kKeySize. length:%zu\n",
                  data.length());
          return false;
        }
        *secret = data;
        return true;
      }
    }
  }
  if (!GenerateRandomData(EncryptModule::kKeySize, secret)) {
    fprintf(stderr, "Error generating base secret data.\n");
    return false;
  }
  return true;
}

// Outputs |data| to a file if |filename| does not already contain any data.
// |filename| path to a file. May be empty. |default_name| path to a file if
// the |filename| was not set. |data| is the data to output. Returns true on
// success.
bool OutputDataToFile(const string& filename,
                      const string& default_name,
                      const string& data) {
  string output_file;
  if (!filename.empty()) {
    FILE* const f = fopen(filename.c_str(), "rb");
    if (f) {
      // File was passed in.
      fclose(f);
    } else {
      output_file = filename;
    }
  } else {
    output_file = default_name;
  }

  // Output the data.
  if (!output_file.empty()) {
    FILE* const f = fopen(output_file.c_str(), "wb");
    if (!f) {
      fprintf(stderr, "Could not open file: %s for writing.\n",
              output_file.c_str());
      return false;
    }

    const size_t bytes_written = fwrite(data.data(), 1, data.size(), f);
    fclose(f);

    if (bytes_written != data.size()) {
      fprintf(stderr, "Error writing to file.\n");
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
      if (name == "content_id") {
        enc->content_id = value;
      } else if (name == "initial_iv") {
        enc->initial_iv = strtoull(value.c_str(), NULL, 10);
      } else if (name == "base_file") {
        enc->base_secret_file = value;
      } else if (name == "unencrypted_range") {
        enc->unencrypted_range = strtoull(value.c_str(), NULL, 10);
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
  mkvparser::MkvReader reader;
  mkvmuxer::MkvWriter writer;
  unique_ptr<mkvparser::Segment> parser_segment;
  unique_ptr<mkvmuxer::Segment> muxer_segment;
  const bool b = OpenWebMFiles(webm_crypt.input,
                               webm_crypt.output,
                               &reader,
                               &parser_segment,
                               &writer,
                               &muxer_segment);
  if (!b) {
    fprintf(stderr, "Could not open WebM files.\n");
    return -1;
  }

  // Set Tracks element attributes
  const mkvparser::Tracks* const parser_tracks = parser_segment->GetTracks();
  uint32_t i = 0;
  uint64_t vid_track = 0;  // no track added
  uint64_t aud_track = 0;  // no track added
  string aud_base_secret;
  string vid_base_secret;

  while (i != parser_tracks->GetTracksCount()) {
    const int track_num = i++;

    const mkvparser::Track* const parser_track =
        parser_tracks->GetTrackByIndex(track_num);

    if (parser_track == NULL)
      continue;

    const char* const track_name = parser_track->GetNameAsUTF8();
    const int64_t track_type = parser_track->GetType();

    if (track_type == mkvparser::Track::kVideo) {
      // Get the video track from the parser
      const mkvparser::VideoTrack* const pVideoTrack =
          reinterpret_cast<const mkvparser::VideoTrack*>(parser_track);
      const int64_t width =  pVideoTrack->GetWidth();
      const int64_t height = pVideoTrack->GetHeight();

      // Add the video track to the muxer. 0 tells muxer to decide on track
      // number.
      vid_track = muxer_segment->AddVideoTrack(static_cast<int>(width),
                                               static_cast<int>(height),
                                               0);
      if (!vid_track) {
        fprintf(stderr, "Could not add video track.\n");
        return -1;
      }

      mkvmuxer::VideoTrack* const video =
          reinterpret_cast<mkvmuxer::VideoTrack*>(
              muxer_segment->GetTrackByNumber(vid_track));
      if (!video) {
        fprintf(stderr, "Could not get video track.\n");
        return -1;
      }

      video->set_codec_id(parser_track->GetCodecId());

      if (track_name)
        video->set_name(track_name);

      const double rate = pVideoTrack->GetFrameRate();
      if (rate > 0.0) {
        video->set_frame_rate(rate);
      }

      if (webm_crypt.video) {
        if (!video->AddContentEncoding()) {
          fprintf(stderr, "Could not add ContentEncoding to video track.\n");
          return -1;
        }

        mkvmuxer::ContentEncoding* const encoding =
            video->GetContentEncodingByIndex(0);
        if (!encoding) {
          fprintf(stderr, "Could not add ContentEncoding to video track.\n");
          return -1;
        }

        mkvmuxer::ContentEncAESSettings* const aes =
            encoding->enc_aes_settings();
        if (!aes) {
          fprintf(stderr, "Error getting video ContentEncAESSettings.\n");
          return -1;
        }
        if (aes->cipher_mode() != mkvmuxer::ContentEncAESSettings::kCTR) {
          fprintf(stderr, "Cipher Mode is not CTR.\n");
          return -1;
        }

        if (!GetBaseSecret(webm_crypt.vid_enc, &vid_base_secret)) {
          fprintf(stderr, "Error generating base secret.\n");
          return -1;
        }

        string id = webm_crypt.vid_enc.content_id;
        if (id.empty()) {
          // If the content id is empty, create a content id.
          if (!GenerateRandomData(EncryptModule::kDefaultContentIDSize, &id)) {
            fprintf(stderr, "Error generating content id data.\n");
            return -1;
          }
        }
        if (!encoding->SetEncryptionID(
                reinterpret_cast<const uint8_t*>(id.data()), id.size())) {
          fprintf(stderr, "Could not set encryption id for video track.\n");
          return -1;
        }
      }
    } else if (track_type == mkvparser::Track::kAudio) {
      // Get the audio track from the parser
      const mkvparser::AudioTrack* const pAudioTrack =
          reinterpret_cast<const mkvparser::AudioTrack*>(parser_track);
      const int64_t channels =  pAudioTrack->GetChannels();
      const double sample_rate = pAudioTrack->GetSamplingRate();

      // Add the audio track to the muxer. 0 tells muxer to decide on track
      // number.
      aud_track = muxer_segment->AddAudioTrack(static_cast<int>(sample_rate),
                                               static_cast<int>(channels),
                                               0);
      if (!aud_track) {
        fprintf(stderr, "Could not add audio track.\n");
        return -1;
      }

      mkvmuxer::AudioTrack* const audio =
          reinterpret_cast<mkvmuxer::AudioTrack*>(
              muxer_segment->GetTrackByNumber(aud_track));
      if (!audio) {
        fprintf(stderr, "Could not get audio track.\n");
        return -1;
      }

      audio->set_codec_id(parser_track->GetCodecId());

      if (track_name)
        audio->set_name(track_name);

      if (pAudioTrack->GetCodecDelay())
        audio->set_codec_delay(pAudioTrack->GetCodecDelay());
      if (pAudioTrack->GetSeekPreRoll())
        audio->set_seek_pre_roll(pAudioTrack->GetSeekPreRoll());

      size_t private_size;
      const uint8_t* const private_data =
          pAudioTrack->GetCodecPrivate(private_size);
      if (private_size > 0) {
        if (!audio->SetCodecPrivate(private_data, private_size)) {
          fprintf(stderr, "Could not add audio private data.\n");
          return -1;
        }
      }

      const int64_t bit_depth = pAudioTrack->GetBitDepth();
      if (bit_depth > 0)
        audio->set_bit_depth(bit_depth);

      if (webm_crypt.audio) {
        if (!audio->AddContentEncoding()) {
          fprintf(stderr, "Could not add ContentEncoding to audio track.\n");
          return -1;
        }

        mkvmuxer::ContentEncoding* const encoding =
            audio->GetContentEncodingByIndex(0);
        if (!encoding) {
          fprintf(stderr, "Could not add ContentEncoding to audio track.\n");
          return -1;
        }

        mkvmuxer::ContentEncAESSettings* const aes =
            encoding->enc_aes_settings();
        if (!aes) {
          fprintf(stderr, "Error getting audio ContentEncAESSettings.\n");
          return -1;
        }
        if (aes->cipher_mode() != mkvmuxer::ContentEncAESSettings::kCTR) {
          fprintf(stderr, "Cipher Mode is not CTR.\n");
          return -1;
        }

        if (!GetBaseSecret(webm_crypt.aud_enc, &aud_base_secret)) {
          fprintf(stderr, "Error generating base secret.\n");
          return -1;
        }

        string id = webm_crypt.aud_enc.content_id;
        if (id.empty()) {
          // If the content id is empty, create a content id.
          if (!GenerateRandomData(EncryptModule::kDefaultContentIDSize, &id)) {
            fprintf(stderr, "Error generating content id data.\n");
            return -1;
          }
        }
        if (!encoding->SetEncryptionID(
                reinterpret_cast<const uint8_t*>(id.data()), id.size())) {
          fprintf(stderr, "Could not set encryption id for video track.\n");
          return -1;
        }
      }
    }
  }

  // Set Cues element attributes
  muxer_segment->CuesTrack(vid_track);

  // Write clusters
  unique_ptr<uint8_t[]> data;
  int data_len = 0;
  EncryptModule audio_encryptor(webm_crypt.aud_enc, aud_base_secret);
  if (webm_crypt.audio && !audio_encryptor.Init()) {
    fprintf(stderr, "Could not initialize audio encryptor.\n");
    return -1;
  }
  audio_encryptor.set_do_not_encrypt(webm_crypt.no_encryption);

  EncryptModule video_encryptor(webm_crypt.vid_enc, vid_base_secret);
  if (webm_crypt.video && !video_encryptor.Init()) {
    fprintf(stderr, "Could not initialize video encryptor.\n");
    return -1;
  }
  video_encryptor.set_do_not_encrypt(webm_crypt.no_encryption);

  const mkvparser::Cluster* prev_cluster = NULL;
  const mkvparser::Cluster* cluster = parser_segment->GetFirst();
  while ((cluster != NULL) && !cluster->EOS()) {
    const mkvparser::BlockEntry* block_entry;
    int status = cluster->GetFirst(block_entry);
    if (status)
      return -1;

    while ((block_entry != NULL) && !block_entry->EOS()) {
      const mkvparser::Block* const block = block_entry->GetBlock();
      const int64_t trackNum = block->GetTrackNumber();
      const mkvparser::Track* const parser_track =
          parser_tracks->GetTrackByNumber(static_cast<uint32_t>(trackNum));
      const int64_t track_type = parser_track->GetType();

      if ((track_type == mkvparser::Track::kAudio) ||
          (track_type == mkvparser::Track::kVideo)) {
        const int frame_count = block->GetFrameCount();
        const int64_t time_ns = block->GetTime(cluster);
        const int64_t time_milli =
            time_ns / webm_tools::kNanosecondsPerMillisecond;
        const bool is_key = block->IsKey();

        for (int i = 0; i < frame_count; ++i) {
          const mkvparser::Block::Frame& frame = block->GetFrame(i);

          if (frame.len > data_len) {
            data.reset(new (std::nothrow) uint8_t[frame.len]);  // NOLINT
            if (!data.get())
              return -1;
            data_len = frame.len;
          }

          if (frame.Read(&reader, data.get()))
            return -1;

          if (webm_crypt.match_src_clusters && prev_cluster != cluster) {
            muxer_segment->ForceNewClusterOnNextFrame();
            prev_cluster = cluster;
          }

          const uint64_t track_num =
              (track_type == mkvparser::Track::kAudio) ? aud_track : vid_track;

          if ((track_type == mkvparser::Track::kVideo && webm_crypt.video) ||
              (track_type == mkvparser::Track::kAudio && webm_crypt.audio) ) {
            // Allocate a larger enough buffer to hold encrypted data.
            unique_ptr<uint8_t[]> ciphertext(
                new uint8_t[frame.len + EncryptModule::kSignalByteSize + EncryptModule::kIVSize]);
            size_t ciphertext_size;
            const bool encrypt_frame =
                time_milli >= webm_crypt.aud_enc.unencrypted_range;
            if (track_type == mkvparser::Track::kAudio) {
              if (!audio_encryptor.ProcessData(data.get(),
                                               frame.len,
                                               encrypt_frame,
                                               ciphertext.get(),
                                               &ciphertext_size)) {
                fprintf(stderr, "Could not encrypt audio data.\n");
                return -1;
              }
            } else {
              const bool encrypt_frame =
                  time_milli >= webm_crypt.vid_enc.unencrypted_range;
              if (!video_encryptor.ProcessData(data.get(),
                                               frame.len,
                                               encrypt_frame,
                                               ciphertext.get(),
                                               &ciphertext_size)) {
                fprintf(stderr, "Could not encrypt video data.\n");
                return -1;
              }
            }

            if (!muxer_segment->AddFrame(
                    ciphertext.get(),
                    ciphertext_size,
                    track_num,
                    time_ns,
                    is_key)) {
              fprintf(stderr, "Could not add encrypted frame.\n");
              return -1;
            }
          } else {
            if (!muxer_segment->AddFrame(data.get(),
                                         frame.len,
                                         track_num,
                                         time_ns,
                                         is_key)) {
              fprintf(stderr, "Could not add frame.\n");
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

  // Output base secret data.
  if (!OutputDataToFile(webm_crypt.aud_enc.base_secret_file,
                        "aud_base_secret.key",
                        aud_base_secret)) {
    fprintf(stderr, "Error writing audio base secret to file.\n");
    return -1;
  }
  if (!OutputDataToFile(webm_crypt.vid_enc.base_secret_file,
                        "vid_base_secret.key",
                        vid_base_secret)) {
    fprintf(stderr, "Error writing video base secret to file.\n");
    return -1;
  }

  writer.Close();
  reader.Close();

  return 0;
}

// Function to decrypt a WebM file. |webm_crypt| encryption settings for
// the source and destination files. Returns 0 on success and <0 for an error.
int WebMDecrypt(const WebMCryptSettings& webm_crypt) {
  mkvparser::MkvReader reader;
  mkvmuxer::MkvWriter writer;
  unique_ptr<mkvparser::Segment> parser_segment;
  unique_ptr<mkvmuxer::Segment> muxer_segment;
  const bool b = OpenWebMFiles(webm_crypt.input,
                               webm_crypt.output,
                               &reader,
                               &parser_segment,
                               &writer,
                               &muxer_segment);
  if (!b) {
    fprintf(stderr, "Could not open WebM files.\n");
    return -1;
  }

  // Set Tracks element attributes
  const mkvparser::Tracks* const parser_tracks = parser_segment->GetTracks();
  uint32_t i = 0;
  uint64_t vid_track = 0;  // no track added
  uint64_t aud_track = 0;  // no track added
  EncryptionSettings aud_enc;
  EncryptionSettings vid_enc;
  bool decrypt_video = false;
  bool decrypt_audio = false;
  string aud_base_secret;
  string vid_base_secret;

  while (i != parser_tracks->GetTracksCount()) {
    const int track_num = i++;

    const mkvparser::Track* const parser_track =
        parser_tracks->GetTrackByIndex(track_num);

    if (parser_track == NULL)
      continue;

    const char* const track_name = parser_track->GetNameAsUTF8();
    const int64_t track_type = parser_track->GetType();

    if (track_type == mkvparser::Track::kVideo) {
      // Get the video track from the parser
      const mkvparser::VideoTrack* const pVideoTrack =
          reinterpret_cast<const mkvparser::VideoTrack*>(parser_track);
      const int64_t width =  pVideoTrack->GetWidth();
      const int64_t height = pVideoTrack->GetHeight();

      // Add the video track to the muxer. 0 tells muxer to decide on track
      // number.
      vid_track = muxer_segment->AddVideoTrack(static_cast<int>(width),
                                               static_cast<int>(height),
                                               0);
      if (!vid_track) {
        fprintf(stderr, "Could not add video track.\n");
        return -1;
      }

      mkvmuxer::VideoTrack* const video =
          reinterpret_cast<mkvmuxer::VideoTrack*>(
              muxer_segment->GetTrackByNumber(vid_track));
      if (!video) {
        fprintf(stderr, "Could not get video track.\n");
        return -1;
      }

      video->set_codec_id(parser_track->GetCodecId());

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
          fprintf(stderr, "Could not get first ContentEncoding.\n");
          return -1;
        }

        if (!ParseContentEncryption(*encoding, &vid_enc)) {
          fprintf(stderr, "Could not parse video ContentEncryption element.\n");
          return -1;
        }

        if (!ReadDataFromFile(webm_crypt.vid_enc.base_secret_file,
                              &vid_base_secret)) {
          fprintf(stderr, "Could not read video base secret file:%s\n",
                  webm_crypt.vid_enc.base_secret_file.c_str());
          return -1;
        }
        decrypt_video = true;
      }

    } else if (track_type == mkvparser::Track::kAudio) {
      // Get the audio track from the parser
      const mkvparser::AudioTrack* const pAudioTrack =
          reinterpret_cast<const mkvparser::AudioTrack*>(parser_track);
      const int64_t channels =  pAudioTrack->GetChannels();
      const double sample_rate = pAudioTrack->GetSamplingRate();

      // Add the audio track to the muxer. 0 tells muxer to decide on track
      // number.
      aud_track = muxer_segment->AddAudioTrack(static_cast<int>(sample_rate),
                                               static_cast<int>(channels),
                                               0);
      if (!aud_track) {
        fprintf(stderr, "Could not add audio track.\n");
        return -1;
      }

      mkvmuxer::AudioTrack* const audio =
          reinterpret_cast<mkvmuxer::AudioTrack*>(
              muxer_segment->GetTrackByNumber(aud_track));
      if (!audio) {
        fprintf(stderr, "Could not get audio track.\n");
        return -1;
      }

      audio->set_codec_id(parser_track->GetCodecId());

      if (track_name)
        audio->set_name(track_name);

      if (pAudioTrack->GetCodecDelay())
        audio->set_codec_delay(pAudioTrack->GetCodecDelay());
      if (pAudioTrack->GetSeekPreRoll())
        audio->set_seek_pre_roll(pAudioTrack->GetSeekPreRoll());

      size_t private_size;
      const uint8_t* const private_data =
          pAudioTrack->GetCodecPrivate(private_size);
      if (private_size > 0) {
        if (!audio->SetCodecPrivate(private_data, private_size)) {
          fprintf(stderr, "Could not add audio private data.\n");
          return -1;
        }
      }

      const int64_t bit_depth = pAudioTrack->GetBitDepth();
      if (bit_depth > 0)
        audio->set_bit_depth(bit_depth);

      // Check if the stream is encrypted.
      const int encoding_count = pAudioTrack->GetContentEncodingCount();
      if (encoding_count > 0) {
        // Only check the first content encoding.
        const ContentEncoding* const encoding =
            pAudioTrack->GetContentEncodingByIndex(0);
        if (!encoding) {
          fprintf(stderr, "Could not get first ContentEncoding.\n");
          return -1;
        }

        if (!ParseContentEncryption(*encoding, &aud_enc)) {
          fprintf(stderr, "Could not parse audio ContentEncryption element.\n");
          return -1;
        }

        if (!ReadDataFromFile(webm_crypt.aud_enc.base_secret_file,
                              &aud_base_secret)) {
          fprintf(stderr, "Could not read audio base secret file:%s\n",
                  webm_crypt.aud_enc.base_secret_file.c_str());
          return -1;
        }
        decrypt_audio = true;
      }
    }
  }

  // Set Cues element attributes
  muxer_segment->CuesTrack(vid_track);

  // Write clusters
  unique_ptr<uint8_t[]> data;
  int data_len = 0;
  DecryptModule audio_decryptor(aud_enc,
                                aud_base_secret,
                                webm_crypt.no_encryption);
  if (decrypt_audio && !audio_decryptor.Init()) {
    fprintf(stderr, "Could not initialize audio decryptor.\n");
    return -1;
  }

  DecryptModule video_decryptor(vid_enc,
                                vid_base_secret,
                                webm_crypt.no_encryption);
  if (decrypt_video && !video_decryptor.Init()) {
    fprintf(stderr, "Could not initialize video decryptor.\n");
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
      const int64_t trackNum = block->GetTrackNumber();
      const mkvparser::Track* const parser_track =
          parser_tracks->GetTrackByNumber(static_cast<uint32_t>(trackNum));
      const int64_t track_type = parser_track->GetType();

      if ((track_type == mkvparser::Track::kAudio) ||
          (track_type == mkvparser::Track::kVideo)) {
        const int frame_count = block->GetFrameCount();
        const int64_t time_ns = block->GetTime(cluster);
        const bool is_key = block->IsKey();

        for (int i = 0; i < frame_count; ++i) {
          const mkvparser::Block::Frame& frame = block->GetFrame(i);

          if (frame.len > data_len) {
            data.reset(new (std::nothrow) uint8_t[frame.len]);  // NOLINT
            if (!data.get())
              return -1;
            data_len = frame.len;
          }

          if (frame.Read(&reader, data.get()))
            return -1;

          const uint64_t track_num =
              (track_type == mkvparser::Track::kAudio) ? aud_track : vid_track;

          if ((track_type == mkvparser::Track::kVideo && decrypt_video) ||
              (track_type == mkvparser::Track::kAudio && decrypt_audio) ) {
            unique_ptr<uint8_t[]> decrypttext(new uint8_t[frame.len]);
            size_t decrypttext_size = 0;
            if (track_type == mkvparser::Track::kAudio) {
              if (!audio_decryptor.DecryptData(data.get(),
                                               frame.len,
                                               decrypttext.get(),
                                               &decrypttext_size)) {
                fprintf(stderr, "Could not decrypt audio data.\n");
                return -1;
              }
            } else {
              if (!video_decryptor.DecryptData(data.get(),
                                               frame.len,
                                               decrypttext.get(),
                                               &decrypttext_size)) {
                fprintf(stderr, "Could not decrypt video data.\n");
                return -1;
              }
            }

            if (decrypttext_size != 0) {
              if (!muxer_segment->AddFrame(
                      decrypttext.get(),
                      decrypttext_size,
                      track_num,
                      time_ns,
                      is_key)) {
                fprintf(stderr, "Could not add encrypted frame.\n");
                return -1;
              }
            }
          } else {
            if (!muxer_segment->AddFrame(data.get(),
                                         frame.len,
                                         track_num,
                                         time_ns,
                                         is_key)) {
              fprintf(stderr, "Could not add frame.\n");
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
    fprintf(stderr, "stream:%s Only CTR cipher mode supported. mode:%s\n",
            name.c_str(),
            enc.cipher_mode.c_str());
    return false;
  }

  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  WebMCryptSettings webm_crypt_settings;
  bool encrypt = true;
  bool test = false;

  // Create initial random IV values.
  if (!GenerateRandomuint64_t(&webm_crypt_settings.aud_enc.initial_iv)) {
    fprintf(stderr, "Could not generate initial IV value.\n");
    return EXIT_FAILURE;
  }
  if (!GenerateRandomuint64_t(&webm_crypt_settings.vid_enc.initial_iv)) {
    fprintf(stderr, "Could not generate initial IV value.\n");
    return EXIT_FAILURE;
  }

  // Parse command line options.
  const int argc_check = argc - 1;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp("-h", argv[i]) || !strcmp("-?", argv[i])) {
      Usage();
      return 0;
    } else if (!strcmp("-v", argv[i])) {
      printf("version: %s\n", WEBM_CRYPT_VERSION_STRING);
    } else if (!strcmp("-i", argv[i]) && i++ < argc_check) {
      webm_crypt_settings.input = argv[i];
    } else if (!strcmp("-o", argv[i]) && i++ < argc_check) {
      webm_crypt_settings.output = argv[i];
    } else if (!strcmp("-audio", argv[i]) && i++ < argc_check) {
      webm_crypt_settings.audio = !strcmp("true", argv[i]);
    } else if (!strcmp("-video", argv[i]) && i++ < argc_check) {
      webm_crypt_settings.video = !strcmp("true", argv[i]);
    } else if (!strcmp("-decrypt", argv[i])) {
      encrypt = false;
    } else if (!strcmp("-no_encryption", argv[i]) && i++ < argc_check) {
      webm_crypt_settings.no_encryption = !strcmp("true", argv[i]);
    } else if (!strcmp("-match_src_clusters", argv[i]) && i++ < argc_check) {
      webm_crypt_settings.match_src_clusters = !strcmp("true", argv[i]);
    } else if (!strcmp("-audio_options", argv[i]) && i++ < argc_check) {
      string option_list(argv[i]);
      ParseStreamOptions(option_list, &webm_crypt_settings.aud_enc);
    } else if (!strcmp("-video_options", argv[i]) && i++ < argc_check) {
      string option_list(argv[i]);
      ParseStreamOptions(option_list, &webm_crypt_settings.vid_enc);
    } else if (!strcmp("-test", argv[i])) {
      test = true;
    } else {
      if (i == argc_check) {
        --i;
        printf("Unknown or invalid parameter. index:%d parameter:%s\n",
               i, argv[i]);
      } else {
        printf("Unknown parameter. index:%d parameter:%s\n", i, argv[i]);
      }
      return EXIT_FAILURE;
    }
  }

  if (test) {
    TestEncryption();
    return EXIT_SUCCESS;
  }

  // Check main parameters.
  if (webm_crypt_settings.input.empty()) {
    fprintf(stderr, "No input file set.\n");
    Usage();
    return EXIT_FAILURE;
  }
  if (webm_crypt_settings.output.empty()) {
    fprintf(stderr, "No output file set.\n");
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
      fprintf(stderr, "Error encrypting WebM file.\n");
      return EXIT_FAILURE;
    }
  } else {
    const int rv = WebMDecrypt(webm_crypt_settings);
    if (rv < 0) {
      fprintf(stderr, "Error decrypting WebM file.\n");
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
