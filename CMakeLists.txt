##  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
cmake_minimum_required(VERSION 2.8)
project(webmtools)

set(WEBMTOOLS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
# TODO(tomfinegan): The libwebm source directory must be configurable.
set(LIBWEBM_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libwebm")
# This is where LIBWEBM targets are built.
set(LIBWEBM_BUILD_DIR "${CMAKE_BINARY_DIR}/libwebm_build")

# Libwebm section.
# Add the libwebm CMakeLists.txt and set the binary dir to allow access to
# libwebm build output.
add_subdirectory("${LIBWEBM_SRC_DIR}" "${LIBWEBM_BUILD_DIR}")
include_directories("${LIBWEBM_SRC_DIR}"
                    "${WEBMTOOLS_SRC_DIR}/shared")

add_library(${PROJECT_NAME} SHARED
  "${WEBMTOOLS_SRC_DIR}/shared/indent.cc"
  "${WEBMTOOLS_SRC_DIR}/shared/webm_chunk_writer.cc"
  "${WEBMTOOLS_SRC_DIR}/shared/webm_endian.cc"
  "${WEBMTOOLS_SRC_DIR}/shared/webm_file.cc"
  "${WEBMTOOLS_SRC_DIR}/shared/webm_incremental_reader.cc"
  "${WEBMTOOLS_SRC_DIR}/shared/webm_live_muxer.cc")

target_link_libraries(${PROJECT_NAME} LINK_PUBLIC webm)
