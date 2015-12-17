// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdint.h>
#include <string>

#include <openssl/aes.h>                                                        
#include <openssl/buffer.h>
#include <openssl/conf.h>                                                       
#include <openssl/err.h>                                                        
#include <openssl/hmac.h>                                                       
#include <openssl/evp.h>                                                        
#include <openssl/rand.h>                                                       

using namespace std;

// This class implements AES-CTR encryption. Only 128-bits Initialization
// Vector is supported in this class.
class AesCtr128Encryptor {
public:
  AesCtr128Encryptor() {}
  ~AesCtr128Encryptor() {}

  bool InitKey(const string& key) {
    if (AES_set_encrypt_key(reinterpret_cast<const unsigned char*>(key.c_str()),
        128, &aes_key_) != 0) {
      return false;
    }
    return true;
  }

  bool SetCounter(const string& counter) {
    if (counter.size() != AES_BLOCK_SIZE)
      return false;
    counter_ = counter;
    return true;
  }
 
  bool Encrypt(const uint8_t* input, size_t size, uint8_t* output) {
    if (counter_.size() == 0) {
      return false;
    }

    uint8_t ivec[AES_BLOCK_SIZE] = { 0 };
    uint8_t ecount_buf[AES_BLOCK_SIZE] = { 0 };
    unsigned int block_offset = 0;

    memcpy(ivec, counter_.c_str(), AES_BLOCK_SIZE);
    AES_ctr128_encrypt(input, output, size, &aes_key_, ivec, ecount_buf,
                       &block_offset);

    return true;
  }

private:
  string counter_;
  AES_KEY aes_key_;
};
