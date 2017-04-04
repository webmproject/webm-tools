// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "IxoDASHManifestParserConstants.h"

// DASH MPD element names.
NSString* const kElementMPD = @"MPD";
NSString* const kElementPeriod = @"Period";
NSString* const kElementAdaptationSet = @"AdaptationSet";
NSString* const kElementRepresentation = @"Representation";
NSString* const kElementBaseURL = @"BaseURL";
NSString* const kElementSegmentBase = @"SegmentBase";
NSString* const kElementInitialization = @"Initialization";
NSString* const kElementAudioChannelConfiguration =
    @"AudioChannelConfiguration";

// DASH MPD element attribute names.
NSString* const kAttributeType = @"type";
NSString* const kAttributeMediaPresentationDuration =
    @"mediaPresentationDuration";
NSString* const kAttributeMinBufferTime = @"minBufferTime";
NSString* const kAttributeID = @"id";
NSString* const kAttributeStart = @"start";
NSString* const kAttributeDuration = @"duration";
NSString* const kAttributeMimeType = @"mimeType";
NSString* const kAttributeCodecs = @"codecs";
NSString* const kAttributeBitstreamSwitching = @"bitstreamSwitching";
NSString* const kAttributeSubsegmentAlignment = @"subsegmentAlignment";
NSString* const kAttributeSubsegmentStartsWithSAP = @"subsegmentStartsWithSAP";
NSString* const kAttributeBandwidth = @"bandwidth";
NSString* const kAttributeWidth = @"width";
NSString* const kAttributeHeight = @"height";
NSString* const kAttributeRange = @"range";
NSString* const kAttributeIndexRange = @"indexRange";
NSString* const kAttributeAudioSamplingRate = @"audioSamplingRate";
NSString* const kAttributeStartWithSAP = @"startWithSAP";
NSString* const kAttributeValue = @"value";

// Known values for MPD elements and attributes.
NSString* const kMimeTypeWebmAudio = @"audio/webm";
NSString* const kMimeTypeWebmVideo = @"video/webm";
NSString* const kCodecVP8 = @"vp8";
NSString* const kCodecVP9 = @"vp9";
NSString* const kCodecOpus = @"opus";
NSString* const kCodecVorbis = @"vorbis";
NSString* const kPresentationTypeDynamic = @"dynamic";
NSString* const kPresentationTypeStatic = @"static";
