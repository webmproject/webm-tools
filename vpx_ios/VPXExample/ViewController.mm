// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#import "ViewController.h"

#include "VPX/vpx/vpx_config.h"
#include "VPX/vpx/vpx_version.h"

#import "GlkVideoViewController.h"
#import "vpx_player.h"

@interface ViewController()<NSURLSessionDelegate,
                            NSURLSessionDownloadDelegate,
                            UITableViewDelegate>
- (void)downloadFileList;
- (void)downloadTestFile;
- (BOOL)copyDownloadedFileToCache:(NSURL *)downloadedFileURL;
- (void)appendToOutputTextView:(NSString *)stringToAppend;
- (void)enableControls;
- (void)disableControls;
@end

@implementation ViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  testFiles_ = nil;
  selectedFileIndex_ = 0;
  [self downloadFileList];

  _downloadButton.enabled = YES;
  [_downloadButton addTarget:self
                      action:@selector(downloadTestFile)
            forControlEvents:UIControlEventTouchUpInside];
  _playButton.enabled = NO;
  cacheDirectoryPath_ = [NSSearchPathForDirectoriesInDomains(
      NSCachesDirectory, NSUserDomainMask, YES) lastObject];
  [self
      appendToOutputTextView:[NSString stringWithFormat:@"libvpx: %s %s",
                                                        VERSION_STRING_NOSP,
                                                        VPX_FRAMEWORK_TARGET]];
#ifdef VPXTEST_LOCAL_PLAYBACK_ONLY
  [self appendToOutputTextView:@"1. Touch a file."];
  [self appendToOutputTextView:@"2. Touch Play"];
#else
  [self appendToOutputTextView:@"1. Touch a file."];
  [self appendToOutputTextView:@"2. Touch Download"];
  [self appendToOutputTextView:@"3. Touch Play"];
#endif  // VPXTEST_LOCAL_PLAYBACK_ONLY
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
}

// Downloads the list of test files used to populate the table view.
- (void)downloadFileList {
#ifdef VPXTEST_LOCAL_PLAYBACK_ONLY
  testFiles_ = [[NSArray alloc] initWithObjects:kVp8File, kVp9File, nil];

  dispatch_async(dispatch_get_main_queue(), ^{
      // Display the file names in testFiles_
      [self.fileList reloadData];

      // Make progress bar label and download button invisible.
      [self.progressLabel setText:@""];
      [self.downloadButton setTitle:@"" forState:UIControlStateDisabled];
      [self.downloadButton setTitle:@"" forState:UIControlStateNormal];
  });
#else
  NSURLSessionConfiguration *session_config =
      [NSURLSessionConfiguration defaultSessionConfiguration];
  NSURLSession *session = [NSURLSession sessionWithConfiguration:session_config
                                                        delegate:self
                                                   delegateQueue:nil];
  NSURLSessionDataTask *dataTask = [session
        dataTaskWithURL:[NSURL URLWithString:TEST_FILE_LIST]
      completionHandler:^(NSData *data, NSURLResponse *response,
                          NSError *error) {
          testFiles_ =
              [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
          if (testFiles_ != nil) {
            dispatch_async(dispatch_get_main_queue(), ^{
                // Reload the table.
                [self.fileList reloadData];
                NSIndexPath *zeroPath =
                    [NSIndexPath indexPathForRow:0 inSection:0];
                // Select the first row.
                [self.fileList
                    selectRowAtIndexPath:zeroPath
                                animated:YES
                          scrollPosition:UITableViewScrollPositionBottom];
            });
          }
      }];
  [dataTask resume];
  NSLog(@"dataTask running");
#endif  // VPXTEST_LOCAL_PLAYBACK_ONLY
}

// Downloads test file.
- (void)downloadTestFile {
#ifndef VPXTEST_LOCAL_PLAYBACK_ONLY
  [self disableControls];
  NSURLSessionConfiguration *session_config =
      [NSURLSessionConfiguration defaultSessionConfiguration];
  NSURLSession *session = [NSURLSession sessionWithConfiguration:session_config
                                                        delegate:self
                                                   delegateQueue:nil];

  downloadFileIndex_ = selectedFileIndex_;
  NSString *fileName = [testFiles_ objectAtIndex:downloadFileIndex_];
  NSString *targetURL =
      [NSString stringWithFormat:@"%@/%@", TEST_SERVER, fileName];

  // TODO(tomfinegan): Skip the download step when the file already exists.

  testFileDownloadURL_ = [NSURL URLWithString:targetURL];
  NSURLSessionDownloadTask *downloadTask =
      [session downloadTaskWithURL:testFileDownloadURL_];
  [downloadTask resume];
  NSLog(@"downloadTask running");
#endif  // VPXTEST_LOCAL_PLAYBACK_ONLY
}

- (void)appendToOutputTextView:(NSString *)stringToAppend {
  NSString *outputTextViewContents = [NSString
      stringWithFormat:@"%@\n%@", _outputTextView.text, stringToAppend];
  _outputTextView.text = outputTextViewContents;
}

// Copies downloaded file from temp directory to cache directory.
- (BOOL)copyDownloadedFileToCache:(NSURL *)downloadedFileURL {
  NSString *fileName = [testFiles_ objectAtIndex:selectedFileIndex_];
  fileName =
      [NSString stringWithFormat:@"%@/%@", cacheDirectoryPath_, fileName];

  // Only copy if the file isn't already there.
  NSError *copyError = nil;
  if ([[NSFileManager defaultManager] fileExistsAtPath:fileName] == NO) {
    if ([[NSFileManager defaultManager] copyItemAtPath:downloadedFileURL.path
                                                toPath:fileName
                                                 error:&copyError] != YES) {
      NSLog(@"Unable to copy %@ to %@ because <<<%@>>>", downloadedFileURL.path,
            fileName, copyError);
    }
  }

  downloadedFilePath_ = [NSURL fileURLWithPath:fileName];
  return [[NSFileManager defaultManager] fileExistsAtPath:fileName];
}

- (void)enableControls {
  _downloadButton.enabled = YES;
  _playButton.enabled = YES;
  _fileList.userInteractionEnabled = YES;
}

- (void)disableControls {
  _downloadButton.enabled = NO;
  _playButton.enabled = NO;
  _fileList.userInteractionEnabled = NO;
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
  if ([segue.identifier isEqualToString:@"Play"]) {
    NSLog(@"Play seque");
    GlkVideoViewController *playerView =
        (GlkVideoViewController *)segue.destinationViewController;
    playerView.fileToPlay = downloadedFilePath_.path;
    playerView.vpxtestViewController = self;
  }
}


//
// NSURLSessionDownloadDelegate implementation
//
- (void)URLSession:(NSURLSession *)session
                 downloadTask:(NSURLSessionDownloadTask *)downloadTask
    didFinishDownloadingToURL:(NSURL *)location {
  if ([self copyDownloadedFileToCache:location]) {
    NSLog(@"Finished downloading %@ to %@", testFileDownloadURL_,
          location.path);
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.progressLabel setText:@"0% - Download another?"];
        [self enableControls];
        [self appendToOutputTextView:
                  [NSString stringWithFormat:
                                @"Ready to play %@.",
                                [testFiles_ objectAtIndex:downloadFileIndex_]]];
    });
  } else {
    NSLog(@"Download failed.");
  }
}

- (void)URLSession:(NSURLSession *)session
          downloadTask:(NSURLSessionDownloadTask *)downloadTask
     didResumeAtOffset:(int64_t)fileOffset
    expectedTotalBytes:(int64_t)expectedTotalBytes {
}

- (void)URLSession:(NSURLSession *)session
                 downloadTask:(NSURLSessionDownloadTask *)downloadTask
                 didWriteData:(int64_t)bytesWritten
            totalBytesWritten:(int64_t)totalBytesWritten
    totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite {
  float progress =
      (double)totalBytesWritten / (double)totalBytesExpectedToWrite;

  dispatch_async(dispatch_get_main_queue(), ^{
      [self.progressView setProgress:progress];
      [self.progressLabel
          setText:[NSString stringWithFormat:@"%.0f%%", progress * 100]];
  });
  // NSLog(@"progress: %.0f", progress * 100);
}

//
// UITableView
//
- (NSInteger)tableView:(UITableView *)tableView
    numberOfRowsInSection:(NSInteger)section {
  return [testFiles_ count];
}
- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  NSString *fileListTableIdentifier = @"FileListTableCell";

  UITableViewCell *cell =
      [tableView dequeueReusableCellWithIdentifier:fileListTableIdentifier];
  if (cell == nil) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                  reuseIdentifier:fileListTableIdentifier];
    UIFont *tinyFont = [UIFont fontWithName:@"Arial" size:14.0];
    cell.textLabel.font = tinyFont;
    cell.textLabel.textAlignment = NSTextAlignmentCenter;
  }

  cell.textLabel.text = [testFiles_ objectAtIndex:indexPath.row];
  return cell;
}

// Tap on table Row
- (void)tableView:(UITableView *)tableView
    didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  selectedFileIndex_ = indexPath.row;
  NSLog(@"touched %lld:%@", static_cast<int64_t>(selectedFileIndex_),
        [testFiles_ objectAtIndex:indexPath.row]);

#ifdef VPXTEST_LOCAL_PLAYBACK_ONLY
  if ([[testFiles_ objectAtIndex:indexPath.row] isEqualToString:kVp8File]) {
    downloadedFilePath_ =
        [NSURL fileURLWithPath:[NSString stringWithFormat:@"%@/%@",
            [[NSBundle mainBundle] bundlePath], kVp8File]];
  } else {
    downloadedFilePath_ =
        [NSURL fileURLWithPath:[NSString stringWithFormat:@"%@/%@",
            [[NSBundle mainBundle] bundlePath], kVp9File]];
  }

  dispatch_async(dispatch_get_main_queue(), ^{
      [self enableControls];
      [self appendToOutputTextView: @"Ready to play."];
  });
#endif
}

//
// GlkVideoViewControllerDelegate
//
- (void)playbackComplete:(BOOL)status statusString:(NSString *)string {
  playFileIndex_ = selectedFileIndex_;

  [self appendToOutputTextView:
      [NSString stringWithFormat:@"Played %@",
          [testFiles_ objectAtIndex:playFileIndex_]]];
  [self appendToOutputTextView:string];
}

@end