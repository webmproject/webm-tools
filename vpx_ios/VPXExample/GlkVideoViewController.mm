// Copyright (c) 2014 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#import "GlkVideoViewController.h"

#include <cassert>
#include <queue>

#import <dispatch/dispatch.h>
#import <OpenGLES/ES2/glext.h>

#import "./vpx_player.h"

namespace {
const NSInteger kRendererFramesPerSecond = 60;

// Uniform index.
enum {
  UNIFORM_Y,
  UNIFORM_UV,
  NUM_UNIFORMS
};
GLint uniforms[NUM_UNIFORMS];

// Attribute index.
enum {
  ATTRIB_VERTEX,
  ATTRIB_TEXCOORD,
  NUM_ATTRIBUTES
};
}  // namespace

struct ViewRectangle {
  ViewRectangle() : view_x(0), view_y(0), view_width(0), view_height(0) {}
  // Origin coordinates.
  float view_x;
  float view_y;

  // View extents from origin coordinates.
  float view_width;
  float view_height;
};

@interface GlkVideoViewController() {
  dispatch_queue_t _playerQueue;
  CVPixelBufferRef *_pixelBuffer;
  NSLock *_lock;
  NSInteger _count;
  std::queue<const VpxExample::VideoBufferPool::VideoBuffer *> _videoBuffers;
  VpxExample::VpxPlayer _vpxPlayer;

  CVOpenGLESTextureCacheRef _videoTextureCache;

  CVOpenGLESTextureRef _yTexture;
  CVOpenGLESTextureRef _uvTexture;

  CGFloat _screenWidth;
  CGFloat _screenHeight;
  size_t _textureWidth;
  size_t _textureHeight;
  ViewRectangle _viewRectangle;

  GLuint _program;
}

@property (strong, nonatomic) EAGLContext *context;
@property (strong, nonatomic) GLKBaseEffect *effect;
@property VpxExample::VpxFormat vpxFormat;

- (const GLfloat *)squareVerticesForCurrentOrientation;
- (const GLfloat *)textureVerticesForCurrentOrientation;
- (void)releaseTextures;
- (void)setupGL;
- (void)teardownGL;
- (BOOL)loadShaders;
- (BOOL)compileShader:(GLuint *)shader type:(GLenum)type file:(NSString *)file;
- (BOOL)linkProgram:(GLuint)prog;
@end  // @interface GlkVideoViewController

@implementation GlkVideoViewController
@synthesize context = _context;
@synthesize fileToPlay = _fileToPlay;
@synthesize vpxtestViewController = _vpxtestViewController;
@synthesize vpxFormat = _vpxFormat;

- (NSInteger)rendererFrameRate {
  return kRendererFramesPerSecond;
}

- (void)playFile {
  bool status = _vpxPlayer.Play();

  // Wait for all buffers to be consumed.
  [_lock lock];
  bool empty = _videoBuffers.empty();
  [_lock unlock];

  while (!empty) {
    [NSThread sleepForTimeInterval:.1];  // 100 milliseconds.
    [_lock lock];
    empty = _videoBuffers.empty();
    [_lock unlock];
  }

  // We're running within our own background queue: Since we're telling the UI
  // to do something, dispatch on the main queue.
  dispatch_async(dispatch_get_main_queue(), ^{
      [_vpxtestViewController playbackComplete:status
                                  statusString:_vpxPlayer.playback_result()];
    self.paused = YES;
    [self dismissViewControllerAnimated:NO completion:nil];
  });
}

- (void)viewDidLoad {
  [super viewDidLoad];

  _count = 0;
  self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

  if (!self.context) {
    NSLog(@"OpenGLES context init failed.");
  }

  _screenWidth = [UIScreen mainScreen].bounds.size.width;
  _screenHeight = [UIScreen mainScreen].bounds.size.height;

  GLKView *view = (GLKView *)self.view;
  view.context = self.context;
  view.drawableMultisample = GLKViewDrawableMultisampleNone;
  self.preferredFramesPerSecond = kRendererFramesPerSecond;

  _lock = [[NSLock alloc] init];
  _playerQueue = dispatch_queue_create("com.google.VPXTest.playerqueue", NULL);
  _vpxPlayer.Init(self);

  if (!_vpxPlayer.LoadFile([_fileToPlay UTF8String])) {
    NSLog(@"File load failed for %@", _fileToPlay);
    return;
  }

  _vpxFormat = _vpxPlayer.vpx_format();

  [self setupGL];

  CVReturn status =
      CVOpenGLESTextureCacheCreate(kCFAllocatorDefault,
                                   NULL,
                                   _context,
                                   NULL,
                                   &_videoTextureCache);
  if (status != kCVReturnSuccess) {
    NSLog(@"CVOpenGLESTextureCacheCreate failed: %d", status);
    return;
  }

  // Create a background queue (thread, basically), and use it for playback.
  dispatch_async(_playerQueue, ^{[self playFile];});
}

- (void)viewDidUnload {
  [super viewDidUnload];
  [self teardownGL];

  if ([EAGLContext currentContext] == _context) {
    [EAGLContext setCurrentContext:nil];
  }
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
}

- (void)releaseTextures {
  if (_yTexture != NULL) {
    CFRelease(_yTexture);
    _yTexture = NULL;
  }
  if (_uvTexture != NULL) {
    CFRelease(_uvTexture);
    _uvTexture = NULL;
  }

  CVOpenGLESTextureCacheFlush(_videoTextureCache, 0);
}

- (void)setupGL {
  [EAGLContext setCurrentContext:_context];

  [self loadShaders];

  glUseProgram(_program);

  glUniform1i(uniforms[UNIFORM_Y], 0);
  glUniform1i(uniforms[UNIFORM_UV], 1);

  //
  // Viewport setup: Calculate a viewport that maintains video aspect ratio.
  // The viewport values can be precalculated, but (at least currently) must be
  // set via glViewport() on every call to :update.
  //
  const CGSize screen_dimensions = [UIScreen mainScreen].bounds.size;
  const CGFloat screen_scale = [[UIScreen mainScreen] scale];

  const float ios_version = [[UIDevice currentDevice].systemVersion floatValue];

  float screen_width = 0;
  float screen_height = 0;

  if (ios_version >= 8.0) {
    // As of iOS 8 the screen dimensions change with orientation.
    screen_width = screen_dimensions.width * screen_scale;
    screen_height = screen_dimensions.height * screen_scale;
  } else {
    // Versions of iOS prior to 8 have the same dimensions regardless of
    // device orientation, so we flip height and width.
    screen_width = screen_dimensions.height * screen_scale;
    screen_height = screen_dimensions.width * screen_scale;
  }

  const float screen_aspect = screen_width / screen_height;

  NSLog(@"Device dimensions (landscape): %.0fx%.0f ar:%f",
        screen_width, screen_height,
        screen_aspect);

  // Default the viewport to screen dimensions. NB: origin is bottom left.
  float view_x = 0;
  float view_y = 0;
  float view_width = screen_width;
  float view_height = screen_height;

  // Calculate video aspect ratio.
  const float fwidth = _vpxFormat.width;
  const float fheight = _vpxFormat.height;
  const float video_aspect = fwidth / fheight;

  NSLog(@"Video dimensions: %.0fx%.0f ar:%f", fwidth, fheight, video_aspect);

  // Calculate the new dimension value, and then update the origin coordinates
  // to center the image (horizontally or vertically; as appropriate).
  // The goal is one dimension equal to device dimension, and the other scaled
  // so that the original image aspect ratio is maintained.
  if (video_aspect >= screen_aspect) {
    view_height = screen_width / video_aspect;
    view_y = (screen_height - view_height) / 2;
  } else {
    view_width = screen_height * video_aspect;
    view_x = (screen_width - view_width) / 2;
  }

  NSLog(@"View x=%f y=%f width=%f height=%f",
        view_x, view_y, view_width, view_height);

  // Save viewport dimensions.
  _viewRectangle.view_x = view_x;
  _viewRectangle.view_y = view_y;
  _viewRectangle.view_width = view_width;
  _viewRectangle.view_height = view_height;
}

- (void)teardownGL{
  [EAGLContext setCurrentContext:_context];

  if (_program) {
    glDeleteProgram(_program);
    _program = 0;
  }
}

// The openGL viewport has floating point coordinates extending from (-1, -1) to
// (1, 1) with a centered origin of (0, 0). Here we return the corners of a
// square that fills the entire viewport.
- (const GLfloat *)squareVerticesForCurrentOrientation {
  UIInterfaceOrientation orientation = [self interfaceOrientation];
  const GLfloat *squareVertices = NULL;
  // Each coordinate pair specifies a corner in the following order:
  //   Top left.
  //   Bottom left.
  //   Top right.
  //   Bottom right.
  if (orientation == UIInterfaceOrientationLandscapeLeft ||
      orientation == UIInterfaceOrientationLandscapeRight) {
    // Use the same coordinates for both landscape orientations-- iOS rotates
    // the viewport with the device.
    static const GLfloat squareVerticesLandscape[] = {
      -1,  1,
      -1, -1,
      1,  1,
      1, -1,
    };
    squareVertices = squareVerticesLandscape;
  } else if (orientation == UIInterfaceOrientationPortrait||
             orientation == UIInterfaceOrientationPortraitUpsideDown) {
    assert(0);
    NSLog(@"Unsupported square UIInterfaceOrientation");
  }
  assert(squareVertices);
  return squareVertices;
}

// Return coordinates that stick a texture to the square defined by
// :squareVerticesForCurrentOrientation. Origin here is top left (0,0) and
// extends to (1, 1), the bottom right.
- (const GLfloat *)textureVerticesForCurrentOrientation {
  UIInterfaceOrientation orientation = [self interfaceOrientation];
  const GLfloat *textureVertices = NULL;
  // Each coordinate pair specifies a corner in the following order:
  //   Top left.
  //   Bottom left.
  //   Top right.
  //   Bottom right.
  if (orientation == UIInterfaceOrientationLandscapeLeft ||
      orientation == UIInterfaceOrientationLandscapeRight) {
    // Use the same coordinates for both landscape orientations-- iOS rotates
    // the viewport with the device.
    static const GLfloat textureVerticesLandscape[] = {
      0, 0,
      0, 1,
      1, 0,
      1, 1,
    };
    textureVertices = textureVerticesLandscape;
  } else {
    assert(0);
    NSLog(@"Unsupported texture UIInterfaceOrientation");
  }
  return textureVertices;
}

// Show a video frame when one is available.
- (void)update {
  // Check for a frame in the queue.
  const VpxExample::VideoBufferPool::VideoBuffer *buffer = NULL;

  if ([_lock tryLock] == YES) {
    if (_videoBuffers.empty()) {
      NSLog(@"buffer queue empty.");
    } else {
      buffer = _videoBuffers.front();
      _videoBuffers.pop();
      NSLog(@"popped buffer.");
    }
    [_lock unlock];
  }

  // NULL buffer means no frame; do nothing.
  if (buffer == NULL)
    return;

  const size_t width = CVPixelBufferGetWidthOfPlane(buffer->buffer, 0);
  const size_t height = CVPixelBufferGetHeightOfPlane(buffer->buffer, 0);

  // Release textures from previous frame. (Can't reuse them,
  // CVOpenGLESTextureCache doesn't provide a method that loads an image
  // into an existing texture).
  [self releaseTextures];

  // Y-Plane texture.
  glActiveTexture(GL_TEXTURE0);
  CVReturn status =
      CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                   _videoTextureCache,
                                                   buffer->buffer,
                                                   NULL,
                                                   GL_TEXTURE_2D,
                                                   GL_RED_EXT,
                                                   static_cast<int>(width),
                                                   static_cast<int>(height),
                                                   GL_RED_EXT,
                                                   GL_UNSIGNED_BYTE,
                                                   0,
                                                   &_yTexture);
  if (status != kCVReturnSuccess) {
    NSLog(@"CVOpenGLESTextureCacheCreateTextureFromImage Y failed: %d", status);
    return;
  }

  glBindTexture(CVOpenGLESTextureGetTarget(_yTexture),
                CVOpenGLESTextureGetName(_yTexture));
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // UV-Plane texture.
  const int uv_width = static_cast<int>(width) / 2;
  const int uv_height = static_cast<int>(height) / 2;

  glActiveTexture(GL_TEXTURE1);
  status =
      CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                   _videoTextureCache,
                                                   buffer->buffer,
                                                   NULL,
                                                   GL_TEXTURE_2D,
                                                   GL_RG_EXT,
                                                   uv_width,
                                                   uv_height,
                                                   GL_RG_EXT,
                                                   GL_UNSIGNED_BYTE,
                                                   1,
                                                   &_uvTexture);
  if (status != kCVReturnSuccess) {
    NSLog(@"CVOpenGLESTextureCacheCreateTextureFromImage UV failed: %d",
          status);
    return;
  }

  glBindTexture(CVOpenGLESTextureGetTarget(_uvTexture),
                CVOpenGLESTextureGetName(_uvTexture));
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Set vertex and texture attributes.
  const GLfloat *squareVertices = [self squareVerticesForCurrentOrientation];
  glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
  glEnableVertexAttribArray(ATTRIB_VERTEX);

  const GLfloat *textureVertices = [self textureVerticesForCurrentOrientation];
  glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, 0, 0, textureVertices);
  glEnableVertexAttribArray(ATTRIB_TEXCOORD);

  // Viewport needs to be set before each draw.
  // TODO(tomfinegan): Investigate relocating texture setup. It would be nice to
  // only do it once per video instead of once per update.
  glViewport(_viewRectangle.view_x,
             _viewRectangle.view_y,
             _viewRectangle.view_width,
             _viewRectangle.view_height);

  // Draw.
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // Return the buffer we just displayed.
  _vpxPlayer.ReleaseVideoBuffer(buffer);
}

// Receives buffers from player and stores them in |_videoBuffers|.
- (void)receiveVideoBuffer:(const void*)videoBuffer {
  [_lock lock];
  typedef VpxExample::VideoBufferPool::VideoBuffer VideoBuffer;
  const VideoBuffer *video_buffer =
      reinterpret_cast<const VideoBuffer *>(videoBuffer);
  _videoBuffers.push(video_buffer);
  NSLog(@"pushed buffer.");
  [_lock unlock];
}

//
// OpenGL ES 2 shader compilation
//
- (BOOL)loadShaders {
  GLuint vertShader, fragShader;
  NSString *vertShaderPathname, *fragShaderPathname;

  // Create shader program.
  _program = glCreateProgram();

  // Create and compile vertex shader.
  vertShaderPathname =
      [[NSBundle mainBundle] pathForResource:@"nv12_vertex_shader"
                                      ofType:@"glsl"];
  if (![self compileShader:&vertShader
                      type:GL_VERTEX_SHADER
                      file:vertShaderPathname]) {
    NSLog(@"Failed to compile vertex shader");
    return NO;
  }

  // Create and compile fragment shader.
  fragShaderPathname =
      [[NSBundle mainBundle] pathForResource:@"nv12_fragment_shader"
                                      ofType:@"glsl"];
  if (![self compileShader:&fragShader
                      type:GL_FRAGMENT_SHADER
                      file:fragShaderPathname]) {
    NSLog(@"Failed to compile fragment shader");
    return NO;
  }

  // Attach vertex shader to program.
  glAttachShader(_program, vertShader);

  // Attach fragment shader to program.
  glAttachShader(_program, fragShader);

  // Bind attribute locations.
  // This needs to be done prior to linking.
  glBindAttribLocation(_program, ATTRIB_VERTEX, "position");
  glBindAttribLocation(_program, ATTRIB_TEXCOORD, "texCoord");

  // Link program.
  if (![self linkProgram:_program]) {
    NSLog(@"Failed to link program: %d", _program);

    if (vertShader) {
      glDeleteShader(vertShader);
      vertShader = 0;
    }
    if (fragShader) {
      glDeleteShader(fragShader);
      fragShader = 0;
    }
    if (_program) {
      glDeleteProgram(_program);
      _program = 0;
    }

    return NO;
  }

  // Get uniform locations.
  uniforms[UNIFORM_Y] = glGetUniformLocation(_program, "SamplerY");
  uniforms[UNIFORM_UV] = glGetUniformLocation(_program, "SamplerUV");

  // Release vertex and fragment shaders.
  if (vertShader) {
    glDetachShader(_program, vertShader);
    glDeleteShader(vertShader);
  }
  if (fragShader) {
    glDetachShader(_program, fragShader);
    glDeleteShader(fragShader);
  }

  return YES;
}

- (BOOL)compileShader:(GLuint *)shader type:(GLenum)type file:(NSString *)file {
  const GLchar *source =
      (GLchar *)[[NSString stringWithContentsOfFile:file
                                           encoding:NSUTF8StringEncoding
                                              error:nil] UTF8String];
  if (source == NULL) {
    NSLog(@"Failed to load vertex shader");
    return NO;
  }

  *shader = glCreateShader(type);
  glShaderSource(*shader, 1, &source, NULL);
  glCompileShader(*shader);

#if !defined(NDEBUG)
  GLint logLength;
  glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength);
  if (logLength > 0) {
    GLchar *log = (GLchar *)malloc(logLength);
    glGetShaderInfoLog(*shader, logLength, &logLength, log);
    NSLog(@"Shader compile log:\n%s", log);
    free(log);
  }
#endif

  GLint status;
  glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
  if (status == 0) {
    glDeleteShader(*shader);
    return NO;
  }

  return YES;
}

- (BOOL)linkProgram:(GLuint)prog {
  GLint status;
  glLinkProgram(prog);

#if !defined(NDEBUG)
  GLint logLength;
  glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
  if (logLength > 0) {
    GLchar *log = (GLchar *)malloc(logLength);
    glGetProgramInfoLog(prog, logLength, &logLength, log);
    NSLog(@"Program link log:\n%s", log);
    free(log);
  }
#endif

  glGetProgramiv(prog, GL_LINK_STATUS, &status);
  if (status == 0) {
    return NO;
  }

  return YES;
}

@end  // @implementation GlkVideoViewController
