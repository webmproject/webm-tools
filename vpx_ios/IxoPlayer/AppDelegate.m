// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "AppDelegate.h"
#import "TextViewController.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  // We're in training wheels mode! Just create a window.

  // Create the window.
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.window.backgroundColor = [UIColor blackColor];
  [self.window makeKeyAndVisible];

  // Create the text view.
  TextViewController* debug_console = [[TextViewController alloc] init];

  // Make our text view the root controller.
  self.window.rootViewController = debug_console;

  // Show the window.
  [self.window makeKeyAndVisible];

  return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application {
  // Sent when the application is about to move from active to inactive state.
  // This can occur for certain types of temporary interruptions (such as an
  // incoming phone call or SMS message) or when the user quits the application
  // and it begins the transition to the background state.
  // Use this method to pause ongoing tasks, disable timers, and throttle down
  // OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
  // Use this method to release shared resources, save user data, invalidate
  // timers, and store enough application state information to restore your
  // application to its current state in case it is terminated later.
  // If your application supports background execution, this method is called
  // instead of applicationWillTerminate: when the user quits.
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
  // Called as part of the transition from the background to the inactive state;
  // here you can undo many of the changes made on entering the background.
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
  // Restart any tasks that were paused (or not yet started) while the
  // application was inactive. If the application was previously in the
  // background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication*)application {
  // Called when the application is about to terminate. Save data if
  // appropriate. See also applicationDidEnterBackground:.
}

@end
