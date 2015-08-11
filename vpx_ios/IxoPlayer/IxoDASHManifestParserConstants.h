// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import <Foundation/Foundation.h>

// DASH MPD element names.
FOUNDATION_EXPORT NSString* const kElementMPD;
FOUNDATION_EXPORT NSString* const kElementPeriod;
FOUNDATION_EXPORT NSString* const kElementAdaptationSet;
FOUNDATION_EXPORT NSString* const kElementRepresentation;
FOUNDATION_EXPORT NSString* const kElementBaseURL;
FOUNDATION_EXPORT NSString* const kElementSegmentBase;
FOUNDATION_EXPORT NSString* const kElementInitialization;
FOUNDATION_EXPORT NSString* const kElementAudioChannelConfiguration;

// DASH MPD element attribute names.
FOUNDATION_EXPORT NSString* const kAttributeType;
FOUNDATION_EXPORT NSString* const kAttributeMediaPresentationDuration;
FOUNDATION_EXPORT NSString* const kAttributeMinBufferTime;
FOUNDATION_EXPORT NSString* const kAttributeID;
FOUNDATION_EXPORT NSString* const kAttributeStart;
FOUNDATION_EXPORT NSString* const kAttributeDuration;
FOUNDATION_EXPORT NSString* const kAttributeMimeType;
FOUNDATION_EXPORT NSString* const kAttributeCodecs;
FOUNDATION_EXPORT NSString* const kAttributeBitstreamSwitching;
FOUNDATION_EXPORT NSString* const kAttributeSubsegmentAlignment;
FOUNDATION_EXPORT NSString* const kAttributeSubsegmentStartsWithSAP;
FOUNDATION_EXPORT NSString* const kAttributeBandwidth;
FOUNDATION_EXPORT NSString* const kAttributeWidth;
FOUNDATION_EXPORT NSString* const kAttributeHeight;
FOUNDATION_EXPORT NSString* const kAttributeRange;
FOUNDATION_EXPORT NSString* const kAttributeIndexRange;
FOUNDATION_EXPORT NSString* const kAttributeAudioSamplingRate;
FOUNDATION_EXPORT NSString* const kAttributeStartWithSAP;
FOUNDATION_EXPORT NSString* const kAttributeValue;

// Known values for MPD elements and attributes.
FOUNDATION_EXPORT NSString* const kMimeTypeWebmAudio;
FOUNDATION_EXPORT NSString* const kMimeTypeWebmVideo;

FOUNDATION_EXPORT NSString* const kCodecOpus;
FOUNDATION_EXPORT NSString* const kCodecVorbis;

FOUNDATION_EXPORT NSString* const kCodecVP8;
FOUNDATION_EXPORT NSString* const kCodecVP9;

FOUNDATION_EXPORT NSString* const kPresentationTypeDynamic;
FOUNDATION_EXPORT NSString* const kPresentationTypeStatic;

// TODO(tomfinegan): Not sure if I'll find it useful to have dictionaries of
// expected attributes for each element. That and whether or not a dictionary
// of elements expected as children of the element being parsed are currently
// open questions.
