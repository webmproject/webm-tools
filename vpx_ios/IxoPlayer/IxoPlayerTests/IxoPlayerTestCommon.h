// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import <Foundation/Foundation.h>

// URLs for test manifest files.
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1URLString;

// Expected values for data length and MD5 checksum (of the entire MPD file).
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1Length;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1MD5;

// Line lengths, their contents, and their offsets (when non-zero) for ranged
// request tests.
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1FirstLineLength;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1FirstLine;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1MiddleLineLength;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1MiddleLineOffset;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1MiddleLine;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1LastLineLength;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1LastLineOffset;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1LastLine;