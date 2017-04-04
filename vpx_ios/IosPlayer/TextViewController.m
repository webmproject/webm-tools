// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "TextViewController.h"

@interface TextViewController ()
@property(nonatomic, strong, readwrite) UITextView* textView;
@property(nonatomic, readwrite) bool touched;
@end

@implementation TextViewController

@synthesize textView;
@synthesize touched;

- (void)viewDidLoad {
  [super viewDidLoad];
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
  // Dispose of any resources that can be recreated.
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  touched = true;
}

- (void)appendText:(NSString*)theText {
  // Lazy init.
  if (textView == nil) {
    UIWindow* main_window = [[UIApplication sharedApplication] delegate].window;
    textView = [[UITextView alloc] initWithFrame:[main_window bounds]];
    if (textView == nil)
      return;

    textView.editable = NO;
    textView.backgroundColor = [UIColor blackColor];
    textView.textColor = [UIColor whiteColor];
    [self.view addSubview:textView];
  }

  // Disable scrolling while updating; this prevents the view from jumping to
  // the bottom because of the update.
  textView.scrollEnabled = NO;
  textView.text = [textView.text stringByAppendingString:theText];
  textView.scrollEnabled = YES;
}
@end
