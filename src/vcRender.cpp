#include "vcRender.h"

#include "gl/vcRenderShaders.h"
#include "gl/vcFramebuffer.h"
#include "gl/vcShader.h"
#include "gl/vcGLState.h"

#include "vcTerrain.h"
#include "vcGIS.h"

#include "gl/opengl/vcOpenGL.h"

const int qrIndices[6] = { 0, 1, 2, 0, 2, 3 };
const vcSimpleVertex qrSqVertices[4]{ { { -1.f, 1.f, 0.f },{ 0, 0 } },{ { -1.f, -1.f, 0.f },{ 0, 1 } },{ { 1.f, -1.f, 0.f },{ 1, 1 } },{ { 1.f, 1.f, 0.f },{ 1, 0 } } };

enum
{
  vcRender_SceneSizeIncrement = 32 // directX framebuffer can only be certain increments
};

struct vcUDRenderContext
{
  vdkRenderContext *pRenderer;
  vdkRenderView *pRenderView;
  uint32_t *pColorBuffer;
  float *pDepthBuffer;
  vcTexture *pColourTex;
  vcTexture *pDepthTex;

  struct
  {
    vcShader *pProgram;
    vcShaderSampler *uniform_texture;
    vcShaderSampler *uniform_depth;
    vcShaderConstantBuffer *uniform_params;

    struct
    {
      udFloat4 screenParams;  // sampleStepX, sampleStepSizeY, near plane, far plane
      udFloat4x4 inverseViewProjection;

      // outlining
      udFloat4 outlineColour;
      udFloat4 outlineParams;   // outlineWidth, threshold, (unused), (unused)

      // colour by height
      udFloat4 colourizeHeightColourMin;
      udFloat4 colourizeHeightColourMax;
      udFloat4 colourizeHeightParams; // min world height, max world height, (unused), (unused)

      // colour by depth
      udFloat4 colourizeDepthColour;
      udFloat4 colourizeDepthParams; // min depth, max depth, (unused), (unused)

      // contours
      udFloat4 contourColour;
      udFloat4 contourParams; // contour distance, contour band height, (unused), (unused)
    } params;

  } presentShader;
};

struct vcRenderContext
{
  vdkContext *pVaultContext;
  vcSettings *pSettings;
  vcCamera *pCamera;

  udUInt2 sceneResolution;
  udUInt2 originalSceneResolution;

  vcFramebuffer *pFramebuffer;
  vcTexture *pTexture;
  vcTexture *pDepthTexture;

  vcUDRenderContext udRenderContext;

  udDouble4x4 viewMatrix;
  udDouble4x4 projectionMatrix;
  udDouble4x4 skyboxProjMatrix;
  udDouble4x4 viewProjectionMatrix;
  udDouble4x4 inverseViewProjectionMatrix;

  vcTerrain *pTerrain;

  struct
  {
    vcShader *pProgram;
    vcShaderSampler *uniform_texture;
    vcShaderConstantBuffer *uniform_MatrixBlock;
  } skyboxShader;

  vcMesh *pScreenQuadMesh;
  vcTexture *pSkyboxCubeMapTexture;
};

udResult vcRender_RecreateUDView(vcRenderContext *pRenderContext);
udResult vcRender_RenderAndUploadUDToTexture(vcRenderContext *pRenderContext, vcRenderData &renderData);

udResult vcRender_Init(vcRenderContext **ppRenderContext, vcSettings *pSettings, vcCamera *pCamera, const udUInt2 &sceneResolution)
{
  udResult result = udR_Success;
  vcRenderContext *pRenderContext = nullptr;

  UD_ERROR_NULL(ppRenderContext, udR_InvalidParameter_);

  pRenderContext = udAllocType(vcRenderContext, 1, udAF_Zero);
  UD_ERROR_NULL(pRenderContext, udR_MemoryAllocationFailure);

  UD_ERROR_IF(!vcShader_CreateFromText(&pRenderContext->udRenderContext.presentShader.pProgram, g_udVertexShader, g_udFragmentShader, vcSimpleVertexLayout, (int)udLengthOf(vcSimpleVertexLayout)), udR_InternalError);
  UD_ERROR_IF(!vcShader_CreateFromText(&pRenderContext->skyboxShader.pProgram, g_udVertexShader, g_vcSkyboxFragmentShader, vcSimpleVertexLayout, (int)udLengthOf(vcSimpleVertexLayout)), udR_InternalError);

  vcMesh_Create(&pRenderContext->pScreenQuadMesh, vcSimpleVertexLayout, 2, qrSqVertices, 4, qrIndices, 6, vcMF_Dynamic);

  vcTexture_LoadCubemap(&pRenderContext->pSkyboxCubeMapTexture, "CloudWater.jpg");

  vcShader_Bind(pRenderContext->skyboxShader.pProgram);
  vcShader_GetSamplerIndex(&pRenderContext->skyboxShader.uniform_texture, pRenderContext->skyboxShader.pProgram, "u_texture");
  vcShader_GetConstantBuffer(&pRenderContext->skyboxShader.uniform_MatrixBlock, pRenderContext->skyboxShader.pProgram, "u_EveryFrame", sizeof(udFloat4x4));

  vcShader_Bind(pRenderContext->udRenderContext.presentShader.pProgram);
  vcShader_GetSamplerIndex(&pRenderContext->udRenderContext.presentShader.uniform_texture, pRenderContext->udRenderContext.presentShader.pProgram, "u_texture");
  vcShader_GetSamplerIndex(&pRenderContext->udRenderContext.presentShader.uniform_depth, pRenderContext->udRenderContext.presentShader.pProgram, "u_depth");
  vcShader_GetConstantBuffer(&pRenderContext->udRenderContext.presentShader.uniform_params, pRenderContext->udRenderContext.presentShader.pProgram, "u_params", sizeof(pRenderContext->udRenderContext.presentShader.params));

  vcShader_Bind(nullptr);

  *ppRenderContext = pRenderContext;

  pRenderContext->pSettings = pSettings;
  pRenderContext->pCamera = pCamera;
  result = vcRender_ResizeScene(pRenderContext, sceneResolution.x, sceneResolution.y);
  if (result != udR_Success)
    goto epilogue;

epilogue:
  return result;
}

udResult vcRender_Destroy(vcRenderContext **ppRenderContext)
{
  if (ppRenderContext == nullptr || *ppRenderContext == nullptr)
    return udR_Success;

  udResult result = udR_Success;
  vcRenderContext *pRenderContext = nullptr;

  UD_ERROR_NULL(ppRenderContext, udR_InvalidParameter_);

  pRenderContext = *ppRenderContext;
  *ppRenderContext = nullptr;

  if (vcTerrain_Destroy(&pRenderContext->pTerrain) != udR_Success)
    UD_ERROR_SET(udR_InternalError);

  if (pRenderContext->pVaultContext != nullptr)
  {
    if (vdkRenderView_Destroy(pRenderContext->pVaultContext, &pRenderContext->udRenderContext.pRenderView) != vE_Success)
      UD_ERROR_SET(udR_InternalError);

    if (vdkRenderContext_Destroy(pRenderContext->pVaultContext, &pRenderContext->udRenderContext.pRenderer) != vE_Success)
      UD_ERROR_SET(udR_InternalError);
  }

  vcShader_DestroyShader(&pRenderContext->udRenderContext.presentShader.pProgram);
  vcShader_DestroyShader(&pRenderContext->skyboxShader.pProgram);

  vcMesh_Destroy(&pRenderContext->pScreenQuadMesh);

  vcTexture_Destroy(&pRenderContext->pSkyboxCubeMapTexture);

  udFree(pRenderContext->udRenderContext.pColorBuffer);
  udFree(pRenderContext->udRenderContext.pDepthBuffer);

epilogue:
  vcTexture_Destroy(&pRenderContext->udRenderContext.pColourTex);
  vcTexture_Destroy(&pRenderContext->udRenderContext.pDepthTex);
  vcTexture_Destroy(&pRenderContext->pTexture);
  vcTexture_Destroy(&pRenderContext->pDepthTexture);
  vcFramebuffer_Destroy(&pRenderContext->pFramebuffer);

  udFree(pRenderContext);
  return result;
}

udResult vcRender_SetVaultContext(vcRenderContext *pRenderContext, vdkContext *pVaultContext)
{
  udResult result = udR_Success;

  UD_ERROR_NULL(pRenderContext, udR_InvalidParameter_);

  pRenderContext->pVaultContext = pVaultContext;

  if (vdkRenderContext_Create(pVaultContext, &pRenderContext->udRenderContext.pRenderer) != vE_Success)
    UD_ERROR_SET(udR_InternalError);

epilogue:
  return result;
}

udResult vcRender_ResizeScene(vcRenderContext *pRenderContext, const uint32_t width, const uint32_t height)
{
  udResult result = udR_Success;
  float fov = pRenderContext->pSettings->camera.fieldOfView;
  float aspect = width / (float)height;
  float zNear = pRenderContext->pSettings->camera.nearPlane;
  float zFar = pRenderContext->pSettings->camera.farPlane;

  uint32_t widthIncr = width + (width % vcRender_SceneSizeIncrement != 0 ? vcRender_SceneSizeIncrement - width % vcRender_SceneSizeIncrement : 0);
  uint32_t heightIncr = height + (height % vcRender_SceneSizeIncrement != 0 ? vcRender_SceneSizeIncrement - height % vcRender_SceneSizeIncrement : 0);

  UD_ERROR_NULL(pRenderContext, udR_InvalidParameter_);
  UD_ERROR_IF(width == 0, udR_InvalidParameter_);
  UD_ERROR_IF(height == 0, udR_InvalidParameter_);

  pRenderContext->sceneResolution.x = widthIncr;
  pRenderContext->sceneResolution.y = heightIncr;
  pRenderContext->originalSceneResolution.x = width;
  pRenderContext->originalSceneResolution.y = height;
  pRenderContext->projectionMatrix = udDouble4x4::perspective(fov, aspect, zNear, zFar);
  pRenderContext->skyboxProjMatrix = udDouble4x4::perspective(fov, aspect, 0.5f, 10000.f);

  //Resize CPU Targets
  udFree(pRenderContext->udRenderContext.pColorBuffer);
  udFree(pRenderContext->udRenderContext.pDepthBuffer);

  pRenderContext->udRenderContext.pColorBuffer = udAllocType(uint32_t, pRenderContext->sceneResolution.x*pRenderContext->sceneResolution.y, udAF_Zero);
  pRenderContext->udRenderContext.pDepthBuffer = udAllocType(float, pRenderContext->sceneResolution.x*pRenderContext->sceneResolution.y, udAF_Zero);

  //Resize GPU Targets
  vcTexture_Destroy(&pRenderContext->udRenderContext.pColourTex);
  vcTexture_Destroy(&pRenderContext->udRenderContext.pDepthTex);
  vcTexture_Create(&pRenderContext->udRenderContext.pColourTex, pRenderContext->sceneResolution.x, pRenderContext->sceneResolution.y, pRenderContext->udRenderContext.pColorBuffer, vcTextureFormat_BGRA8, vcTFM_Nearest, false, vcTWM_Repeat, vcTCF_Dynamic);
  vcTexture_Create(&pRenderContext->udRenderContext.pDepthTex, pRenderContext->sceneResolution.x, pRenderContext->sceneResolution.y, pRenderContext->udRenderContext.pDepthBuffer, vcTextureFormat_D32F, vcTFM_Nearest, false, vcTWM_Repeat, vcTCF_Dynamic);

  vcTexture_Destroy(&pRenderContext->pTexture);
  vcTexture_Destroy(&pRenderContext->pDepthTexture);
  vcFramebuffer_Destroy(&pRenderContext->pFramebuffer);
  vcTexture_Create(&pRenderContext->pTexture, widthIncr, heightIncr, nullptr, vcTextureFormat_RGBA8, vcTFM_Nearest, false, vcTWM_Repeat, vcTCF_RenderTarget);
  vcTexture_Create(&pRenderContext->pDepthTexture, widthIncr, heightIncr, nullptr, vcTextureFormat_D32F, vcTFM_Nearest, false, vcTWM_Repeat, vcTCF_RenderTarget);
  vcFramebuffer_Create(&pRenderContext->pFramebuffer, pRenderContext->pTexture, pRenderContext->pDepthTexture);

  if (pRenderContext->pVaultContext)
    vcRender_RecreateUDView(pRenderContext);

epilogue:
  return result;
}

void vcRenderSkybox(vcRenderContext *pRenderContext)
{
  // Draw the skybox only at the far plane, where there is no geometry.
  // Drawing skybox here (after 'opaque' geometry) saves a bit on fill rate.

  udFloat4x4 viewMatrixF = udFloat4x4::create(pRenderContext->viewMatrix);
  udFloat4x4 projectionMatrixF = udFloat4x4::create(pRenderContext->skyboxProjMatrix);
  udFloat4x4 inverseViewProjMatrixF = projectionMatrixF * viewMatrixF;
  inverseViewProjMatrixF.axis.t = udFloat4::create(0, 0, 0, 1);
  inverseViewProjMatrixF.inverse();

  vcGLState_SetViewport(0, 0, pRenderContext->sceneResolution.x, pRenderContext->sceneResolution.y, 1.0f, 1.0f);
  VERIFY_GL();

  vcShader_Bind(pRenderContext->skyboxShader.pProgram);
  VERIFY_GL();

  vcShader_BindTexture(pRenderContext->skyboxShader.pProgram, pRenderContext->pSkyboxCubeMapTexture, 0, pRenderContext->skyboxShader.uniform_texture);
  vcShader_BindConstantBuffer(pRenderContext->skyboxShader.pProgram, pRenderContext->skyboxShader.uniform_MatrixBlock, &inverseViewProjMatrixF, sizeof(udDouble4x4));
  VERIFY_GL();

  vcMesh_RenderTriangles(pRenderContext->pScreenQuadMesh, 2);

  vcGLState_SetViewport(0, 0, pRenderContext->sceneResolution.x, pRenderContext->sceneResolution.y, 0.0f, 1.0f);

  vcShader_Bind(nullptr);
}

void vcPresentUD(vcRenderContext *pRenderContext)
{
  // edge outlines
  int outlineWidth = pRenderContext->pSettings->postVisualization.edgeOutlines.width;
  float outlineEdgeThreshold = pRenderContext->pSettings->postVisualization.edgeOutlines.threshold;
  udFloat4 outlineColour = pRenderContext->pSettings->postVisualization.edgeOutlines.colour;
  if (!pRenderContext->pSettings->postVisualization.edgeOutlines.enable)
    outlineColour.w = 0.0f;

  // colour by height
  udFloat4 colourByHeightMinColour = pRenderContext->pSettings->postVisualization.colourByHeight.minColour;
  if (!pRenderContext->pSettings->postVisualization.colourByHeight.enable)
    colourByHeightMinColour.w = 0.f;
  udFloat4 colourByHeightMaxColour = pRenderContext->pSettings->postVisualization.colourByHeight.maxColour;
  if (!pRenderContext->pSettings->postVisualization.colourByHeight.enable)
    colourByHeightMaxColour.w = 0.f;
  float colourByHeightStartHeight = pRenderContext->pSettings->postVisualization.colourByHeight.startHeight;
  float colourByHeightEndHeight = pRenderContext->pSettings->postVisualization.colourByHeight.endHeight;

  // colour by depth
  udFloat4 colourByDepthColour = pRenderContext->pSettings->postVisualization.colourByDepth.colour;
  if (!pRenderContext->pSettings->postVisualization.colourByDepth.enable)
    colourByDepthColour.w = 0.f;
  float colourByDepthStart = pRenderContext->pSettings->postVisualization.colourByDepth.startDepth;
  float colourByDepthEnd = pRenderContext->pSettings->postVisualization.colourByDepth.endDepth;

  // contours
  udFloat4 contourColour = pRenderContext->pSettings->postVisualization.contours.colour;
  if (!pRenderContext->pSettings->postVisualization.contours.enable)
    contourColour.w = 0.f;
  float contourDistances = pRenderContext->pSettings->postVisualization.contours.distances;
  float contourBandHeight = pRenderContext->pSettings->postVisualization.contours.bandHeight;

  pRenderContext->udRenderContext.presentShader.params.inverseViewProjection = udFloat4x4::create(pRenderContext->inverseViewProjectionMatrix);
  pRenderContext->udRenderContext.presentShader.params.screenParams.x = outlineWidth * (1.0f / pRenderContext->pSettings->window.width);
  pRenderContext->udRenderContext.presentShader.params.screenParams.y = outlineWidth * (1.0f / pRenderContext->pSettings->window.height);
  pRenderContext->udRenderContext.presentShader.params.screenParams.z = pRenderContext->pSettings->camera.nearPlane;
  pRenderContext->udRenderContext.presentShader.params.screenParams.w = pRenderContext->pSettings->camera.farPlane;
  pRenderContext->udRenderContext.presentShader.params.outlineColour = outlineColour;
  pRenderContext->udRenderContext.presentShader.params.outlineParams.x = (float)outlineWidth;
  pRenderContext->udRenderContext.presentShader.params.outlineParams.y = outlineEdgeThreshold;
  pRenderContext->udRenderContext.presentShader.params.colourizeHeightColourMin = colourByHeightMinColour;
  pRenderContext->udRenderContext.presentShader.params.colourizeHeightColourMax = colourByHeightMaxColour;
  pRenderContext->udRenderContext.presentShader.params.colourizeHeightParams.x = colourByHeightStartHeight;
  pRenderContext->udRenderContext.presentShader.params.colourizeHeightParams.y = colourByHeightEndHeight;
  pRenderContext->udRenderContext.presentShader.params.colourizeDepthColour = colourByDepthColour;
  pRenderContext->udRenderContext.presentShader.params.colourizeDepthParams.x = colourByDepthStart;
  pRenderContext->udRenderContext.presentShader.params.colourizeDepthParams.y = colourByDepthEnd;
  pRenderContext->udRenderContext.presentShader.params.contourColour = contourColour;
  pRenderContext->udRenderContext.presentShader.params.contourParams.x = contourDistances;
  pRenderContext->udRenderContext.presentShader.params.contourParams.y = contourBandHeight;

  vcShader_Bind(pRenderContext->udRenderContext.presentShader.pProgram);

  vcShader_BindTexture(pRenderContext->udRenderContext.presentShader.pProgram, pRenderContext->udRenderContext.pColourTex, 0, pRenderContext->udRenderContext.presentShader.uniform_texture);
  vcShader_BindTexture(pRenderContext->udRenderContext.presentShader.pProgram, pRenderContext->udRenderContext.pDepthTex, 1, pRenderContext->udRenderContext.presentShader.uniform_depth);
  vcShader_BindConstantBuffer(pRenderContext->udRenderContext.presentShader.pProgram, pRenderContext->udRenderContext.presentShader.uniform_params, &pRenderContext->udRenderContext.presentShader.params, sizeof(pRenderContext->udRenderContext.presentShader.params));

  vcMesh_RenderTriangles(pRenderContext->pScreenQuadMesh, 2);

  vcShader_Bind(nullptr);
}

vcTexture* vcRender_RenderScene(vcRenderContext *pRenderContext, vcRenderData &renderData, vcFramebuffer *pDefaultFramebuffer)
{
  float fov = pRenderContext->pSettings->camera.fieldOfView;
  float aspect = pRenderContext->sceneResolution.x / (float)pRenderContext->sceneResolution.y;
  float zNear = pRenderContext->pSettings->camera.nearPlane;
  float zFar = pRenderContext->pSettings->camera.farPlane;

  pRenderContext->viewMatrix = renderData.cameraMatrix;
  pRenderContext->viewMatrix.inverse();

  pRenderContext->projectionMatrix = udDouble4x4::perspective(fov, aspect, zNear, zFar);
  pRenderContext->skyboxProjMatrix = udDouble4x4::perspective(fov, aspect, 0.5f, 10000.f);

  pRenderContext->viewProjectionMatrix = pRenderContext->projectionMatrix * pRenderContext->viewMatrix;
  pRenderContext->inverseViewProjectionMatrix = udInverse(pRenderContext->viewProjectionMatrix);

  vcGLState_SetDepthMode(vcGLSDM_LessOrEqual, true);

  vcRender_RenderAndUploadUDToTexture(pRenderContext, renderData);

  vcGLState_SetViewport(0, 0, pRenderContext->sceneResolution.x, pRenderContext->sceneResolution.y);

  vcFramebuffer_Bind(pRenderContext->pFramebuffer);
  vcFramebuffer_Clear(pRenderContext->pFramebuffer, 0xFFFF8080);

  vcPresentUD(pRenderContext);

  vcRenderSkybox(pRenderContext);

  if (vcGIS_AcceptableSRID(renderData.srid) && pRenderContext->pSettings->maptiles.mapEnabled)
  {
    udDouble4x4 cameraMatrix = renderData.cameraMatrix;

#ifndef GIT_BUILD
    static bool debugDetachCamera = false;
    static udDouble4x4 gRealCameraMatrix = udDouble4x4::identity();
    if (!debugDetachCamera)
      gRealCameraMatrix = renderData.cameraMatrix;

    cameraMatrix = gRealCameraMatrix;
#endif
    udDouble3 localCamPos = cameraMatrix.axis.t.toVector3();

    // Corners [nw, ne, sw, se]
    udDouble3 localCorners[4];
    udInt2 slippyCorners[4];

    int currentZoom = 21;

    // Cardinal Limits
    localCorners[0] = localCamPos + udDouble3::create(-pRenderContext->pSettings->camera.farPlane, +pRenderContext->pSettings->camera.farPlane, 0);
    localCorners[1] = localCamPos + udDouble3::create(+pRenderContext->pSettings->camera.farPlane, +pRenderContext->pSettings->camera.farPlane, 0);
    localCorners[2] = localCamPos + udDouble3::create(-pRenderContext->pSettings->camera.farPlane, -pRenderContext->pSettings->camera.farPlane, 0);
    localCorners[3] = localCamPos + udDouble3::create(+pRenderContext->pSettings->camera.farPlane, -pRenderContext->pSettings->camera.farPlane, 0);

    for (int i = 0; i < 4; ++i)
      vcGIS_LocalToSlippy(renderData.srid, &slippyCorners[i], localCorners[i], currentZoom);

    while (currentZoom > 0 && (slippyCorners[0] != slippyCorners[1] || slippyCorners[1] != slippyCorners[2] || slippyCorners[2] != slippyCorners[3]))
    {
      --currentZoom;

      for (int i = 0; i < 4; ++i)
        slippyCorners[i] /= 2;
    }

    for (int i = 0; i < 4; ++i)
      vcGIS_SlippyToLocal(renderData.srid, &localCorners[i], slippyCorners[0] + udInt2::create(i & 1, i / 2), currentZoom);

    // for now just rebuild terrain every frame
    vcTerrain_BuildTerrain(pRenderContext->pTerrain, renderData.srid, localCorners, udInt3::create(slippyCorners[0], currentZoom), localCamPos, pRenderContext->viewProjectionMatrix);
    vcTerrain_Render(pRenderContext->pTerrain, pRenderContext->viewMatrix, pRenderContext->projectionMatrix);
  }

  vcShader_Bind(nullptr);

  vcFramebuffer_Bind(pDefaultFramebuffer);

  return pRenderContext->pTexture;
}

udResult vcRender_RecreateUDView(vcRenderContext *pRenderContext)
{
  udResult result = udR_Success;

  UD_ERROR_NULL(pRenderContext, udR_InvalidParameter_);

  if (pRenderContext->udRenderContext.pRenderView && vdkRenderView_Destroy(pRenderContext->pVaultContext, &pRenderContext->udRenderContext.pRenderView) != vE_Success)
    UD_ERROR_SET(udR_InternalError);

  if (vdkRenderView_Create(pRenderContext->pVaultContext, &pRenderContext->udRenderContext.pRenderView, pRenderContext->udRenderContext.pRenderer, pRenderContext->sceneResolution.x, pRenderContext->sceneResolution.y) != vE_Success)
    UD_ERROR_SET(udR_InternalError);

  if (vdkRenderView_SetTargets(pRenderContext->pVaultContext, pRenderContext->udRenderContext.pRenderView, pRenderContext->udRenderContext.pColorBuffer, 0, pRenderContext->udRenderContext.pDepthBuffer) != vE_Success)
    UD_ERROR_SET(udR_InternalError);

  if (vdkRenderView_SetMatrix(pRenderContext->pVaultContext, pRenderContext->udRenderContext.pRenderView, vdkRVM_Projection, pRenderContext->projectionMatrix.a) != vE_Success)
    UD_ERROR_SET(udR_InternalError);

epilogue:
  return result;
}

udResult vcRender_RenderAndUploadUDToTexture(vcRenderContext *pRenderContext, vcRenderData &renderData)
{
  udResult result = udR_Success;
  vdkModel **ppModels = nullptr;
  int numVisibleModels = 0;

  if (vdkRenderView_SetMatrix(pRenderContext->pVaultContext, pRenderContext->udRenderContext.pRenderView, vdkRVM_Projection, pRenderContext->projectionMatrix.a) != vE_Success)
    UD_ERROR_SET(udR_InternalError);

  if (vdkRenderView_SetMatrix(pRenderContext->pVaultContext, pRenderContext->udRenderContext.pRenderView, vdkRVM_View, pRenderContext->viewMatrix.a) != vE_Success)
    UD_ERROR_SET(udR_InternalError);

  switch (pRenderContext->pSettings->visualization.mode)
  {
  case vcVM_Intensity:
    vdkRenderContext_ShowIntensity(pRenderContext->pVaultContext, pRenderContext->udRenderContext.pRenderer, pRenderContext->pSettings->visualization.minIntensity, pRenderContext->pSettings->visualization.maxIntensity);
    break;
  case vcVM_Classification:
    vdkRenderContext_ShowClassification(pRenderContext->pVaultContext, pRenderContext->udRenderContext.pRenderer, nullptr, 0);
    break;
  default: //Includes vcVM_Colour
    vdkRenderContext_ShowColor(pRenderContext->pVaultContext, pRenderContext->udRenderContext.pRenderer);
    break;
  }

  if (renderData.models.length > 0)
    ppModels = udAllocStack(vdkModel*, renderData.models.length, udAF_None);

  for (size_t i = 0; i < renderData.models.length; ++i)
  {
    if (renderData.models[i]->modelVisible)
    {
      ppModels[numVisibleModels] = renderData.models[i]->pVaultModel;
      ++numVisibleModels;
    }
  }

  vdkRenderPicking picking;
  picking.x = (uint32_t)((float)renderData.mouse.x / (float)pRenderContext->originalSceneResolution.x * (float)pRenderContext->sceneResolution.x);
  picking.y = (uint32_t)((float)renderData.mouse.y / (float)pRenderContext->originalSceneResolution.y * (float)pRenderContext->sceneResolution.y);

  if (vdkRenderContext_RenderAdv(pRenderContext->pVaultContext, pRenderContext->udRenderContext.pRenderer, pRenderContext->udRenderContext.pRenderView, ppModels, (int)numVisibleModels, &picking) != vE_Success)
    UD_ERROR_SET(udR_InternalError);

  if (picking.hit != 0)
  {
    // More to be done here
    renderData.worldMousePos = udDouble3::create(picking.pointCenter[0], picking.pointCenter[1], picking.pointCenter[2]);
    renderData.pickingSuccess = true;
  }

  vcTexture_UploadPixels(pRenderContext->udRenderContext.pColourTex, pRenderContext->udRenderContext.pColorBuffer, pRenderContext->sceneResolution.x, pRenderContext->sceneResolution.y);
  vcTexture_UploadPixels(pRenderContext->udRenderContext.pDepthTex, pRenderContext->udRenderContext.pDepthBuffer, pRenderContext->sceneResolution.x, pRenderContext->sceneResolution.y);

epilogue:
  udFreeStack(pModels);
  return result;
}

udResult vcRender_CreateTerrain(vcRenderContext *pRenderContext, vcSettings *pSettings)
{
  if (pRenderContext == nullptr || pSettings == nullptr)
    return udR_InvalidParameter_;

  udResult result = udR_Success;

  if (vcTerrain_Init(&pRenderContext->pTerrain, pSettings) != udR_Success)
    UD_ERROR_SET(udR_InternalError);

epilogue:
  return result;
}

udResult vcRender_DestroyTerrain(vcRenderContext *pRenderContext)
{
  if (pRenderContext->pTerrain == nullptr)
    return udR_Success;
  else
    return vcTerrain_Destroy(&(pRenderContext->pTerrain));
}
