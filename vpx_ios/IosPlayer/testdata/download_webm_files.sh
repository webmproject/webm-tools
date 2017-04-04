#!/bin/bash
##
##  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
. ../../../shared/common.sh

readonly BASE_URL="http://demos.webmproject.org/dash/201410/vp9_files"

# File names and their checksums.
readonly FILES=(feelings_vp9-20130806-171.webm
                feelings_vp9-20130806-172.webm
                feelings_vp9-20130806-242.webm
                feelings_vp9-20130806-243.webm
                feelings_vp9-20130806-244.webm
                feelings_vp9-20130806-245.webm
                feelings_vp9-20130806-246.webm
                feelings_vp9-20130806-247.webm)
readonly CHECKSUMS=(ce3e7a2e72bf291850492d104c7cb1e8
                    84a01fd3706db778f954fcb36b0a28ea
                    67d99a812352ba73b2b2493517b1f4e5
                    aeb0ef893cc470211d4edcb27731c114
                    b42feafeaabbbb17a60b7d1b0bcfb385
                    00ca4625c541a7bc69003ad900a2986b
                    bab04a396855d339a607d13a7ee2ea7a
                    937da5127417cfe5ea03cf001f7a8c77)

check_tool curl --version
check_dir testdata

if [[ "${#FILES[@]}" -ne "${#CHECKSUMS[@]}" ]]; then
  elog "Number of files and checksums do not match, doing nothing."
  exit 1
fi

download_and_verify_webm_files() {
  local file=""
  local expected_checksum=""
  local actual_checksum=""
  for (( i = 0; i < ${#FILES[@]}; ++i )); do
    file="${FILES[$i]}"
    expected_checksum="${CHECKSUMS[$i]}"

    vlog "Downloading ${file}..."
    eval curl -O ${BASE_URL}/${FILES[$i]} ${devnull}

    vlog "Verifying ${file}..."
    actual_checksum=$(md5 -q "${file}")
    if [[ "${actual_checksum}" != "${expected_checksum}" ]]; then
      elog "Checksum mismatch for ${file}."
      elog "  expected: ${expected_checksum} actual: ${actual_checksum}"
      exit 1
    fi

    vlog "Done."
  done
}

download_and_verify_webm_files

