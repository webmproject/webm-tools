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

check_tool webm_info -h
check_dir testdata

if [[ -z "$1" ]]; then
  elog "Usage: $0 <WebM file>"
  exit 1
fi

if [[ ! -f "$1" ]]; then
  elog "Unable to open $1"
  exit 1
fi

print_cluster_pos_array() {
  local file="$1"
  local cluster_start_pos_str=$(webm_info -all -i "$file" \
    | grep "Cue Point" \
    | awk -F ':' '{print $NF}')
  local cluster_start_pos_array=(${cluster_start_pos_str})
  local last_array_index=0
  local file_size=$(wc -c < "${file}" | tr -d [:space:])
  local num_clusters=${#cluster_start_pos_array[@]}
  for (( i=1; i < ${num_clusters}; i++ )); do
    last_array_index=$(( $i - 1 ))
    echo @\[@${cluster_start_pos_array[${last_array_index}]}, \
      @${cluster_start_pos_array[$i]}\],
  done
  local last_cluster_index=$(( ${num_clusters} - 1 ))
  echo @\[@${cluster_start_pos_array[${last_cluster_index}]}, @${file_size}\],
}

print_cluster_pos_array "$1"
