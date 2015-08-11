// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import <Foundation/Foundation.h>

FOUNDATION_EXPORT const int kNumElementsInRangeArray;

//
// Constants for tests using testdata/manifest_vp9_vorbis.mpd.
//
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

// DASH manifest parsing result constants.
FOUNDATION_EXPORT const bool kVP9VorbisDASHMPD1IsStatic;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1MediaPresentationDuration;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1MinBufferTime;

// Period constants.
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1PeriodID;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1PeriodStart;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1PeriodDuration;

// Audio AdaptationSet constants.
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1AudioASCount;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1AudioASID;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1AudioASCodecs;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1AudioASMimeType;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1AudioASAudioSamplingRate;

// Audio Representation count and Representation 0 constants.
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1AudioRepCount;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1AudioRep0ID;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1AudioRep0BaseURLString;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1AudioRep0IndexRangeStart;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1AudioRep0IndexRangeEnd;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1AudioRep0InitRangeStart;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1AudioRep0InitRangeEnd;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1AudioRep0Bandwidth;

// Video AdaptationSet constants.
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1VideoASCount;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1VideoASID;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1VideoASCodecs;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1VideoASMimeType;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1VideoASWidth;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1VideoASHeight;

// Video Representation count and Representation 0 constants.
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1VideoRepCount;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1VideoRep0ID;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD1VideoRep0BaseURLString;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1VideoRep0IndexRangeStart;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1VideoRep0IndexRangeEnd;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1VideoRep0InitRangeStart;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1VideoRep0InitRangeEnd;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1VideoRep0Width;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1VideoRep0Height;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD1VideoRep0Bandwidth;

//
// Constants for tests using testdata/manifest_vp8_vorbis.mpd.
//
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2URLString;

// DASH manifest parsing result constants.
FOUNDATION_EXPORT const bool kVP9VorbisDASHMPD2IsStatic;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2MediaPresentationDuration;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2MinBufferTime;

// Period constants.
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2PeriodID;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2PeriodStart;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2PeriodDuration;

// Audio AdaptationSet constants.
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2AudioASCount;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2AudioASID;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2AudioASCodecs;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2AudioASMimeType;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2AudioASAudioSamplingRate;

// Audio Representation count and Representation 0 constants.
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2AudioRepCount;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2AudioRep0ID;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2AudioRep0BaseURLString;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2AudioRep0IndexRangeStart;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2AudioRep0IndexRangeEnd;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2AudioRep0InitRangeStart;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2AudioRep0InitRangeEnd;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2AudioRep0Bandwidth;

// Video AdaptationSet constants.
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2VideoASCount;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2VideoASID;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2VideoASCodecs;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2VideoASMimeType;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2VideoASWidth;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2VideoASHeight;

// Video Representation count and Representation 2 constants.
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2VideoRepCount;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2VideoRep2ID;
FOUNDATION_EXPORT NSString* const kVP9VorbisDASHMPD2VideoRep2BaseURLString;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2VideoRep2IndexRangeStart;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2VideoRep2IndexRangeEnd;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2VideoRep2InitRangeStart;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2VideoRep2InitRangeEnd;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2VideoRep2Width;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2VideoRep2Height;
FOUNDATION_EXPORT const int kVP9VorbisDASHMPD2VideoRep2Bandwidth;