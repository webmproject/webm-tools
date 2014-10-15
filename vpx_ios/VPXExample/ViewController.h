// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef VPX_IOS_VPXEXAMPLE_VIEWCONTROLLER_H_
#define VPX_IOS_VPXEXAMPLE_VIEWCONTROLLER_H_

#import <GLKit/GLKit.h>
#import <UIKit/UIKit.h>

#import "GlkVideoViewController.h"

@interface ViewController : UIViewController<GlkVideoViewControllerDelegate,
                                             UITableViewDataSource> {
 @private
  NSInteger downloadFileIndex_;
  NSInteger playFileIndex_;
  NSInteger selectedFileIndex_;
  NSArray *testFiles_;
  NSURL *testFileDownloadURL_;
  NSURL *cacheDirectoryPath_;
  NSURL *downloadedFilePath_;
}

@property(nonatomic, weak) IBOutlet UIButton *downloadButton;
@property(nonatomic, weak) IBOutlet UIButton *playButton;
@property(nonatomic, weak) IBOutlet UILabel *progressLabel;
@property(nonatomic, weak) IBOutlet UIProgressView *progressView;
@property(nonatomic, weak) IBOutlet UITableView *fileList;
@property(nonatomic, weak) IBOutlet UITextView *outputTextView;
@end

#endif  // VPX_IOS_VPXEXAMPLE_VIEWCONTROLLER_H_
