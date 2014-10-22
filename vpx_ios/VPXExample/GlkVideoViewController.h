// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef VPX_IOS_VPXEXAMPLE_GLKVIDEOVIEWCONTROLLER_H_
#define VPX_IOS_VPXEXAMPLE_GLKVIDEOVIEWCONTROLLER_H_

#include <CoreVideo/CoreVideo.h>
#import <GLKit/GLKit.h>

@protocol GlkVideoViewControllerDelegate<NSObject>
- (void) playbackComplete:(BOOL)status statusString:(NSString *)string;
@end

@class ViewController;

@interface GlkVideoViewController : GLKViewController
@property(nonatomic, weak)
    id<GlkVideoViewControllerDelegate> vpxtestViewController;
@property(nonatomic, strong) NSString *fileToPlay;
@property(readonly) NSInteger rendererFrameRate;

- (void)receiveVideoBuffer:(const void*)videoBuffer
withTimestampInMilliseconds:(int64_t)timestamp;
@end

#endif  // VPX_IOS_VPXEXAMPLE_GLKVIDEOVIEWCONTROLLER_H_

