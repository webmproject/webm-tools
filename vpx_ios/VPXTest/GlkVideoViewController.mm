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

#import "vpx_player.h"

namespace {
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

enum SquareVertexCoordinates {
  kTopLeftX = 0,
  kTopLeftY = 1,
  kBottomLeftX = 2,
  kBottomLeftY = 3,
  kTopRightX = 4,
  kTopRightY = 5,
  kBottomRightX = 6,
  kBottomRightY = 7,
  kNumVertices = 8,
};

// Wraps CVPixelBufferRefs for handling clean up.
class CVPixelBufferWrap {
 public:
  CVPixelBufferWrap() : buffer_(NULL) {}
  ~CVPixelBufferWrap() { if (buffer_ != NULL) CVPixelBufferRelease(buffer_); }
  void set_buffer(CVPixelBufferRef buffer) { buffer_ = buffer; }
  CVPixelBufferRef buffer() const { return buffer_; }
 private:
  CVPixelBufferRef buffer_;
};
}  // namespace

@interface GlkVideoViewController() {
  dispatch_queue_t _playerQueue;
  CVPixelBufferRef *_pixelBuffer;
  NSLock *_lock;
  NSInteger _count;
  std::queue<CVPixelBufferRef> _videoBuffers;
  VpxTest::VpxPlayer _vpxPlayer;

  CVOpenGLESTextureCacheRef _videoTextureCache;

  CVOpenGLESTextureRef _yTexture;
  CVOpenGLESTextureRef _uvTexture;

  CGFloat _screenWidth;
  CGFloat _screenHeight;
  size_t _textureWidth;
  size_t _textureHeight;

  GLuint _program;
}

@property (strong, nonatomic) EAGLContext *context;
@property (strong, nonatomic) GLKBaseEffect *effect;

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

- (void)playFile {
  bool status = _vpxPlayer.PlayFile([_fileToPlay UTF8String]);

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
  view.contentScaleFactor = 1.0;
  self.preferredFramesPerSecond = 60;

  _lock = [[NSLock alloc] init];
  _playerQueue = dispatch_queue_create("com.google.VPXTest.playerqueue", NULL);
  _vpxPlayer.Init(self);

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
  CVPixelBufferWrap buffer;

  if ([_lock tryLock] == YES) {
    if (_videoBuffers.empty()) {
      NSLog(@"buffer queue empty.");
    } else {
      buffer.set_buffer(_videoBuffers.front());
      _videoBuffers.pop();
      NSLog(@"popped buffer.");
    }
    [_lock unlock];
  }

  // NULL buffer means no frame; do nothing.
  if (buffer.buffer() == NULL)
    return;

  const size_t width = CVPixelBufferGetWidthOfPlane(buffer.buffer(), 0);
  const size_t height = CVPixelBufferGetHeightOfPlane(buffer.buffer(), 0);

  // Release textures from previous frame. (Can't reuse them,
  // CVOpenGLESTextureCache doesn't provide a method that loads an image
  // into an existing texture).
  [self releaseTextures];

  // Y-Plane texture.
  glActiveTexture(GL_TEXTURE0);
  CVReturn status =
      CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                   _videoTextureCache,
                                                   buffer.buffer(),
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
                                                   buffer.buffer(),
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

  // Adjust the vertices so that we display the image with proper aspect ratio.
  GLfloat verticesForAspectRatio[8];
  memcpy(verticesForAspectRatio, squareVertices,
         sizeof(verticesForAspectRatio));

  for (int i = 0; i < kNumVertices; ++i)
    verticesForAspectRatio[i] = squareVertices[i];

  const float fwidth = width;
  const float fheight = height;

  // TODO(tomfinegan): Calculating this every frame is a waste; precalculate it.
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad) {
    if (fwidth / fheight > 4.0 / 3.0) {
      // Width:height aspect ratio of clip is larger than the device; scale
      // vertical texture coordinates.
      const float scaleFactor = 1 / (fwidth / fheight);
      for (int i = kTopLeftY; i < kNumVertices; i += 2)
        verticesForAspectRatio[i] *= scaleFactor;
    } else {
      const float scaleFactor = 1 / (fheight / fwidth);
      for (int i = kTopLeftX; i < kNumVertices; i += 2)
        verticesForAspectRatio[i] *= scaleFactor;
    }
  } else {
    if (fwidth / fheight >= 16.0 / 9.0) {
      // Width:height aspect ratio of clip is larger than the device; scale
      // vertical texture coordinates.
      const float scaleFactor = 1 / (fheight / fwidth);
      for (int i = kTopLeftY; i < kNumVertices; i += 2)
        verticesForAspectRatio[i] *= scaleFactor;
    } else {
      const float scaleFactor = 1 / (fwidth / fheight);
      for (int i = kTopLeftX; i < kNumVertices; i += 2)
        verticesForAspectRatio[i] *= scaleFactor;
    }
  }

  glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0,
                        verticesForAspectRatio);

  glEnableVertexAttribArray(ATTRIB_VERTEX);

  const GLfloat *textureVertices = [self textureVerticesForCurrentOrientation];
  glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, 0, 0, textureVertices);
  glEnableVertexAttribArray(ATTRIB_TEXCOORD);

  // Draw.
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// Receives buffers from player and stores them in |_videoBuffers|.
- (void)receivePixelBuffer:(CVPixelBufferRef)pixelBuffer {
  [_lock lock];
  _videoBuffers.push(pixelBuffer);
  //NSLog(@"%@", pixelBuffer);
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