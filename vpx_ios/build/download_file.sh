#!/bin/sh
##
##  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
readonly FILES="sintel_trailer_vp8.webm sintel_trailer_vp9.webm"
readonly HTTP_PATH="http://downloads.webmproject.org/tomfinegan"
readonly DOWNLOAD_PATH="${PROJECT_DIR}/testdata"

if [ ! -d "${DOWNLOAD_PATH}" ]; then
  mkdir -p "${DOWNLOAD_PATH}"
fi

for f in ${FILES}; do
  OUTPUT_FILE="${DOWNLOAD_PATH}/$f"
  if [ ! -e "${OUTPUT_FILE}" ]; then
    curl -o "${OUTPUT_FILE}" "${HTTP_PATH}/$f"
  else
    touch "${OUTPUT_FILE}"
  fi
done
