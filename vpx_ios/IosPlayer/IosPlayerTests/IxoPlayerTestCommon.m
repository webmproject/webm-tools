// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IosPlayerTestCommon.h"

#import "IxoDASHManifestParserConstants.h"
#import "IxoDASHManifestTestData.h"

const int kNumElementsInRangeArray = 2;

//
// Constants for tests using testdata/manifest_vp9_vorbis.mpd.
//
NSString* const kVP9VorbisDASHMPD1URLString =
    @"http://localhost:8000/manifest_vp9_vorbis.mpd";

// Expected values for data length and MD5 checksum (of the entire MPD file).
const int kVP9VorbisDASHMPD1Length = 2005;
NSString* const kVP9VorbisDASHMPD1MD5 = @"8ab746aecdc021ef92d9162f4ba5dd89";

// Line lengths, their contents, and their offsets (when non-zero) for ranged
// request tests.
const int kVP9VorbisDASHMPD1FirstLineLength = 38;
NSString* const kVP9VorbisDASHMPD1FirstLine =
    @"<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

const int kVP9VorbisDASHMPD1MiddleLineLength = 15;
const int kVP9VorbisDASHMPD1MiddleLineOffset = 1108;
NSString* const kVP9VorbisDASHMPD1MiddleLine = @"<Initialization";

const int kVP9VorbisDASHMPD1LastLineLength = 6;
const int kVP9VorbisDASHMPD1LastLineOffset = 1998;
NSString* const kVP9VorbisDASHMPD1LastLine = @"</MPD>";

//
// Constants for tests using testdata/manifest_vp8_vorbis.mpd
//
NSString* const kVP8VorbisDASHMPD1URLString =
    @"http://localhost:8000/manifest_vp8_vorbis.mpd";

NSString* const kVP9VorbisRepCodecsDASHMPD1URLString =
    @"http://localhost:8000/manifest_vp9_vorbis_rep_codecs.mpd";
