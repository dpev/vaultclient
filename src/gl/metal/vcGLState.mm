#import "vcMetal.h"

#import "SDL2/SDL.h"
#import "SDL2/SDL_syswm.h"

#if UDPLATFORM_IOS || UDPLATFORM_IOS_SIMULATOR
#import <UIKit/UIKit.h>
#elif UDPLATFORM_OSX
#import <Cocoa/Cocoa.h>
#else
# error "Unsupported platform!"
#endif

#import <Metal/Metal.h>
#import <MetalKit/MTKView.h>

#include "vcViewCon.h"
#include "vcStrings.h"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_ex/imgui_impl_metal.h"

int32_t g_maxAnisotropy = 0;
vcViewCon *_viewCon;

id<MTLDevice> _device;
id<MTLLibrary> _library;
id<MTLCommandQueue> _queue;

static vcGLState s_internalState;

MTLStencilOperation mapStencilOperation[] =
{
  MTLStencilOperationKeep,
  MTLStencilOperationZero,
  MTLStencilOperationReplace,
  MTLStencilOperationIncrementClamp,
  MTLStencilOperationDecrementClamp
  // Or Inc/Dec Wrap?
};

MTLCompareFunction mapStencilFunction[] =
{
  MTLCompareFunctionAlways,
  MTLCompareFunctionNever,
  MTLCompareFunctionLess,
  MTLCompareFunctionLessEqual,
  MTLCompareFunctionGreater,
  MTLCompareFunctionGreaterEqual,
  MTLCompareFunctionEqual,
  MTLCompareFunctionNotEqual
};

MTLCompareFunction mapDepthMode[] =
{
  MTLCompareFunctionNever,
  MTLCompareFunctionLess,
  MTLCompareFunctionLessEqual,
  MTLCompareFunctionEqual,
  MTLCompareFunctionGreaterEqual,
  MTLCompareFunctionGreater,
  MTLCompareFunctionNotEqual,
  MTLCompareFunctionAlways
};

void vcGLState_BuildDepthStates()
{
  MTLDepthStencilDescriptor *depthStencilDesc = [[MTLDepthStencilDescriptor alloc]init];
  id<MTLDepthStencilState> dsState;

  for (int i = 0 ; i <= vcGLSDM_Always; ++i)
  {
    int j = i * 2;
    depthStencilDesc.depthWriteEnabled = false;
    depthStencilDesc.depthCompareFunction = mapDepthMode[i];
    dsState = [_device newDepthStencilStateWithDescriptor:depthStencilDesc];
    _viewCon.renderer.depthStates[j] = dsState;

    depthStencilDesc.depthWriteEnabled = true;
    dsState = [_device newDepthStencilStateWithDescriptor:depthStencilDesc];
    _viewCon.renderer.depthStates[j+1] = dsState;
  }
}

bool vcGLState_Init(SDL_Window *pWindow, vcFramebuffer **ppDefaultFramebuffer)
{
  _device = MTLCreateSystemDefaultDevice();
  _library = [_device newLibraryWithFile:[[NSBundle mainBundle] pathForResource:@"shaders" ofType:@"metallib" ] error:nil];

  if (_device == nullptr)
  {
    NSLog(@"Metal is not supported on this device");
    return false;
  }
  else if (_library == nullptr)
  {
    NSLog(@"Shader library couldn't be created");
    return false;
  }

  SDL_SysWMinfo wminfo;
  SDL_VERSION(&wminfo.version);
  SDL_GetWindowWMInfo(pWindow, &wminfo);

#if UDPLATFORM_IOS || UDPLATFORM_IOS_SIMULATOR
  UIView *sdlview = wminfo.info.uikit.window;
#elif UDPLATFORM_OSX
  NSView *sdlview = wminfo.info.cocoa.window.contentView;
#else
# error "Unsupported platform!"
#endif

  sdlview.autoresizesSubviews = true;

  _viewCon = [vcViewCon alloc];
  _viewCon.Mview = [[MTKView alloc] initWithFrame:sdlview.frame device:_device];
  if(_viewCon.Mview == nullptr)
  {
      NSLog(@"MTKView wasn't created");
      return false;
  }
  [sdlview addSubview:_viewCon.Mview];
  _viewCon.Mview.device = _device;
  _viewCon.Mview.framebufferOnly = false;
  _viewCon.Mview.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
  _viewCon.Mview.autoResizeDrawable = true;
  _viewCon.Mview.preferredFramesPerSecond = 60;

#if UDPLATFORM_IOS || UDPLATFORM_IOS_SIMULATOR
  _viewCon.Mview.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
#elif UDPLATFORM_OSX
  _viewCon.Mview.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
#else
# error "Unsupported platform!"
#endif

  // Overloading NSViewController/UIViewController function, initializes the view controller objects
  [_viewCon viewDidLoad];

  vcTexture *defaultTexture, *defaultDepth;
  vcTexture_Create(&defaultTexture, sdlview.frame.size.width, sdlview.frame.size.height, nullptr, vcTextureFormat_BGRA8, vcTFM_Nearest, false, vcTWM_Clamp, vcTCF_RenderTarget);
  vcTexture_Create(&defaultDepth, sdlview.frame.size.width, sdlview.frame.size.height, nullptr, vcTextureFormat_D24S8, vcTFM_Nearest, false, vcTWM_Clamp, vcTCF_RenderTarget);

  _viewCon.renderer.renderPasses[0] = _viewCon.Mview.currentRenderPassDescriptor;
  _viewCon.renderer.renderPasses[0].colorAttachments[0].loadAction = MTLLoadActionClear;
  _viewCon.renderer.renderPasses[0].colorAttachments[0].storeAction = MTLStoreActionStore;
  _viewCon.renderer.renderPasses[0].colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
  _viewCon.renderer.renderPasses[0].depthAttachment.loadAction = MTLLoadActionClear;
  _viewCon.renderer.renderPasses[0].depthAttachment.storeAction = MTLStoreActionStore;
  _viewCon.renderer.renderPasses[0].depthAttachment.clearDepth = 1.0;
  _viewCon.renderer.renderPasses[0].stencilAttachment.clearStencil = 0;

  vcFramebuffer *pFramebuffer;
  vcFramebuffer_Create(&pFramebuffer, defaultTexture, defaultDepth);
  vcFramebuffer_Bind(pFramebuffer);

  vcGLState_BuildDepthStates();

  ImGui_ImplMetal_Init();

  *ppDefaultFramebuffer = pFramebuffer;

  s_internalState.viewportZone = udInt4::create(0,0,sdlview.frame.size.width, sdlview.frame.size.height);

  vcGLState_ResetState(true);

  return true;
}

void vcGLState_Deinit()
{
  [_viewCon.Mview removeFromSuperview];
}

bool vcGLState_ApplyState(vcGLState *pState)
{
  bool success = true;

  success &= vcGLState_SetFaceMode(pState->fillMode, pState->cullMode, pState->isFrontCCW);
  success &= vcGLState_SetBlendMode(pState->blendMode);
  success &= vcGLState_SetDepthStencilMode(pState->depthReadMode, pState->doDepthWrite, &pState->stencil);

  return success;
}

bool vcGLState_ResetState(bool force /*= false*/)
{
  bool success = true;

  success &= vcGLState_SetFaceMode(vcGLSFM_Solid, vcGLSCM_Back, true, force);
  success &= vcGLState_SetBlendMode(vcGLSBM_None, force);
  success &= vcGLState_SetDepthStencilMode(vcGLSDM_LessOrEqual, true, nullptr, force);

  return success;
}

bool vcGLState_SetFaceMode(vcGLStateFillMode fillMode, vcGLStateCullMode cullMode, bool isFrontCCW /*= true*/, bool force /*= false*/)
{
  if (force || fillMode != s_internalState.fillMode || cullMode != s_internalState.cullMode || isFrontCCW != s_internalState.isFrontCCW)
  {
    switch(fillMode)
    {
      case vcGLSFM_Solid:
        [_viewCon.renderer setFillMode:MTLTriangleFillModeFill];
        break;
      case vcGLSFM_Wireframe:
        [_viewCon.renderer setFillMode:MTLTriangleFillModeLines];
        break;
      case vcGLSFM_TotalModes:
        return false;
        break;
    }

    switch(cullMode)
    {
      case vcGLSCM_None:
        [_viewCon.renderer setCullMode:MTLCullModeNone];
        break;
      case vcGLSCM_Front:
        [_viewCon.renderer setCullMode:MTLCullModeFront];
        break;
      case vcGLSCM_Back:
        [_viewCon.renderer setCullMode:MTLCullModeBack];
        break;
      case vcGLSCM_TotalModes:
        return false;
        break;
    }

    if (isFrontCCW)
      [_viewCon.renderer setWindingMode:MTLWindingCounterClockwise];
    else
      [_viewCon.renderer setWindingMode:MTLWindingClockwise];

    s_internalState.fillMode = fillMode;
    s_internalState.cullMode = cullMode;
    s_internalState.isFrontCCW = isFrontCCW;
  }
  return true;
}

bool vcGLState_SetBlendMode(vcGLStateBlendMode blendMode, bool force /*= false*/)
{
  if (force || blendMode != s_internalState.blendMode)
  {
    s_internalState.blendMode = blendMode;
    [_viewCon.renderer bindBlendState:blendMode];
  }
  return true;
}

bool vcGLState_SetDepthStencilMode(vcGLStateDepthMode depthReadMode, bool doDepthWrite, vcGLStencilSettings *pStencil /* = nullptr */, bool force /*= false*/)
{
  bool enableStencil = pStencil != nullptr;

  if ((s_internalState.depthReadMode != depthReadMode) || (s_internalState.doDepthWrite != doDepthWrite) || force || (s_internalState.stencil.enabled != enableStencil) ||
      (enableStencil && ((s_internalState.stencil.onStencilFail != pStencil->onStencilFail) || (s_internalState.stencil.onDepthFail != pStencil->onDepthFail) || (s_internalState.stencil.onStencilAndDepthPass != pStencil->onStencilAndDepthPass) || (s_internalState.stencil.compareFunc != pStencil->compareFunc) || (s_internalState.stencil.compareMask != pStencil->compareMask) || (s_internalState.stencil.writeMask != pStencil->writeMask))))
  {
    s_internalState.depthReadMode = depthReadMode;
    s_internalState.doDepthWrite = doDepthWrite;

    s_internalState.stencil.enabled = enableStencil;

    if (!enableStencil)
    {
      [_viewCon.renderer bindDepthStencil:_viewCon.renderer.depthStates[((int)depthReadMode) * 2 + doDepthWrite] settings:nullptr];
      return true;
    }

    MTLDepthStencilDescriptor *depthStencilDesc = [[MTLDepthStencilDescriptor alloc]init];

    depthStencilDesc.depthCompareFunction = mapDepthMode[depthReadMode];
    depthStencilDesc.depthWriteEnabled = doDepthWrite;

#if UDPLATFORM_OSX
    MTLStencilDescriptor *stencilDesc = [[MTLStencilDescriptor alloc] init];

    stencilDesc.readMask = (uint32)pStencil->compareMask;
    stencilDesc.writeMask = (uint32)pStencil->writeMask;
    stencilDesc.stencilCompareFunction = mapStencilFunction[pStencil->compareFunc];
    stencilDesc.stencilFailureOperation = mapStencilOperation[pStencil->onStencilFail];
    stencilDesc.depthFailureOperation = mapStencilOperation[pStencil->onDepthFail];
    stencilDesc.depthStencilPassOperation = mapStencilOperation[pStencil->onStencilAndDepthPass];

    depthStencilDesc.frontFaceStencil = stencilDesc;
    depthStencilDesc.backFaceStencil = stencilDesc;
#elif UDPLATFORM_IOS || UDPLATFORM_IOS_SIMULATOR
    return false;
#else
# error "Unknown platform!"
#endif
    s_internalState.stencil.writeMask = pStencil->writeMask;
    s_internalState.stencil.compareFunc = pStencil->compareFunc;
    s_internalState.stencil.compareValue = pStencil->compareValue;
    s_internalState.stencil.compareMask = pStencil->compareMask;
    s_internalState.stencil.onStencilFail = pStencil->onStencilFail;
    s_internalState.stencil.onDepthFail = pStencil->onDepthFail;
    s_internalState.stencil.onStencilAndDepthPass = pStencil->onStencilAndDepthPass;

    id<MTLDepthStencilState> dsState = [_device newDepthStencilStateWithDescriptor:depthStencilDesc];
    [_viewCon.renderer bindDepthStencil:dsState settings:pStencil];
  }

  return true;
}

bool vcGLState_SetViewport(int32_t x, int32_t y, int32_t width, int32_t height, float minDepth /*= 0.f*/, float maxDepth /*= 1.f*/)
{
  if (x < 0 || y < 0 || width < 1 || height < 1)
    return false;

  MTLViewport vp =
  {
    .originX = double(x),
    .originY = double(y),
    .width = double(width),
    .height = double(height),
    .znear = minDepth,
    .zfar = maxDepth
  };

  [_viewCon.renderer bindViewport:vp];
  s_internalState.viewportZone = udInt4::create(x, y, width, height);

  vcGLState_Scissor(x, y, width + x, height + y);

  return true;
}

bool vcGLState_SetViewportDepthRange(float minDepth, float maxDepth)
{
  return vcGLState_SetViewport(s_internalState.viewportZone.x, s_internalState.viewportZone.y, s_internalState.viewportZone.z, s_internalState.viewportZone.w, minDepth, maxDepth);
}

bool vcGLState_Present(SDL_Window *pWindow)
{
  if (pWindow == nullptr)
    return false;

  @autoreleasepool {
    [_viewCon.Mview draw];
  }

  memset(&s_internalState.frameInfo, 0, sizeof(s_internalState.frameInfo));
  return true;
}

bool vcGLState_ResizeBackBuffer(const uint32_t width, const uint32_t height)
{
  vcGLState_SetViewport(0, 0, width, height);
  return true;
}

void vcGLState_Scissor(int left, int top, int right, int bottom, bool force /*= false*/)
{
  udUnused(force);
  if ((NSUInteger)right > _viewCon.renderer.renderPasses[0].colorAttachments[0].texture.width || (NSUInteger)bottom > _viewCon.renderer.renderPasses[0].colorAttachments[0].texture.height)
    return;
  
  udInt4 newScissor = udInt4::create(left, s_internalState.viewportZone.w - bottom, right - left, bottom - top);
  MTLScissorRect rect = {
      .x = (NSUInteger)left,
      .y = (NSUInteger)top,
      .width = (NSUInteger)right - left,
      .height = (NSUInteger)bottom - top
  };
  [_viewCon.renderer setScissor:rect];
  s_internalState.scissorZone = newScissor;
}

int32_t vcGLState_GetMaxAnisotropy(int32_t desiredAniLevel)
{
  return udMin(desiredAniLevel, g_maxAnisotropy);
}

void vcGLState_ReportGPUWork(size_t drawCount, size_t triCount, size_t uploadBytesCount)
{
  s_internalState.frameInfo.drawCount += drawCount;
  s_internalState.frameInfo.triCount += triCount;
  s_internalState.frameInfo.uploadBytesCount += uploadBytesCount;
}

bool vcGLState_IsGPUDataUploadAllowed()
{
  return (s_internalState.frameInfo.uploadBytesCount < vcGLState_MaxUploadBytesPerFrame);
}