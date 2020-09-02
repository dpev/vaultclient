#include "vcPlaceLayer.h"

#include "vcState.h"
#include "vcRender.h"
#include "vcStrings.h"

#include "vcFenceRenderer.h"
#include "vcInternalModels.h"

#include "udMath.h"
#include "udFile.h"
#include "udStringUtil.h"

#include "imgui.h"
#include "imgui_ex/vcImGuiSimpleWidgets.h"

struct vcPolygonModelLoadInfo
{
  vcState *pProgramState;
  vcPlaceLayer *pNode;
};

void vcPlaceLayer_LoadModel(void *pLoadInfoPtr)
{
  vcPolygonModelLoadInfo *pLoadInfo = (vcPolygonModelLoadInfo *)pLoadInfoPtr;
  if (pLoadInfo->pProgramState->programComplete)
    return;

  udInterlockedCompareExchange(&pLoadInfo->pNode->m_loadStatus, vcSLS_Loading, vcSLS_Pending);
  udResult result = vcPolygonModel_CreateFromURL(&pLoadInfo->pNode->m_pModel, pLoadInfo->pNode->m_pNode->pURI, pLoadInfo->pProgramState->pWorkerPool);

  if (result == udR_OpenFailure)
    result = vcPolygonModel_CreateFromURL(&pLoadInfo->pNode->m_pModel, udTempStr("%s%s", pLoadInfo->pProgramState->activeProject.pRelativeBase, pLoadInfo->pNode->m_pNode->pURI), pLoadInfo->pProgramState->pWorkerPool);

  udInterlockedCompareExchange(&pLoadInfo->pNode->m_loadStatus, (result == udR_Success ? vcSLS_Loaded : vcSLS_Failed), vcSLS_Loading);
}

vcPlaceLayer::vcPlaceLayer(vcProject *pProject, udProjectNode *pNode, vcState *pProgramState) :
  vcSceneItem(pProject, pNode, pProgramState)
{
  m_places.Init(512);

  m_closeLabels.Init(32);
  m_closeLines.Init(32);

  vcFenceRendererConfig config = {};
  config.visualMode = vcRRVM_Flat;
  config.imageMode = vcRRIM_Glow;
  config.isDualColour = false;
  config.ribbonWidth = 3.0;
  config.primaryColour = udFloat4::one();
  config.textureRepeatScale = 1.0;
  config.textureScrollSpeed = 1.0;

  m_loadStatus = vcSLS_Pending;

  vcPolygonModelLoadInfo *pLoadInfo = udAllocType(vcPolygonModelLoadInfo, 1, udAF_Zero);
  if (pLoadInfo != nullptr)
  {
    // Prepare the load info
    pLoadInfo->pNode = this;
    pLoadInfo->pProgramState = pProgramState;

    udWorkerPool_AddTask(pProgramState->pWorkerPool, vcPlaceLayer_LoadModel, pLoadInfo, true);
  }
  else
  {
    m_loadStatus = vcSLS_Failed;
  }

  OnNodeUpdate(pProgramState);

}

void vcPlaceLayer::OnNodeUpdate(vcState *pProgramState)
{
  m_places.Clear();

  udProjectNode_GetMetadataString(m_pNode, "pin", &m_pPinIcon, nullptr);

  udProjectNode_GetMetadataDouble(m_pNode, "labelDistance", &m_labelDistance, 20000);
  udProjectNode_GetMetadataDouble(m_pNode, "pinDistance", &m_pinDistance, 6000000);


  Place place;

  while (udProjectNode_GetMetadataString(m_pNode, udTempStr("places[%zu].name", m_places.length), &place.pName, nullptr) == udE_Success)
  {
    udProjectNode_GetMetadataDouble(m_pNode, udTempStr("places[%zu].lla[0]", m_places.length), &place.latLongAlt.x, 0.0);
    udProjectNode_GetMetadataDouble(m_pNode, udTempStr("places[%zu].lla[1]", m_places.length), &place.latLongAlt.y, 0.0);
    udProjectNode_GetMetadataDouble(m_pNode, udTempStr("places[%zu].lla[2]", m_places.length), &place.latLongAlt.z, 0.0);
    udProjectNode_GetMetadataInt(m_pNode, udTempStr("places[%zu].count", m_places.length), &place.count, 1);

    m_places.PushBack(place);
  }

  ChangeProjection(pProgramState->geozone);
}

void vcPlaceLayer::AddToScene(vcState *pProgramState, vcRenderData *pRenderData)
{
  if (!m_visible)
    return;

  size_t usedLabels = 0;
  vcPolygonModel *pPolygonModel = nullptr;

  for (size_t i = 0; i < m_places.length; ++i)
  {
    double distSq = udMagSq(pProgramState->pActiveViewport->camera.position - m_places[i].localSpace);

    // Labels
    if (distSq < m_labelDistance * m_labelDistance)
    {
      if (m_closeLabels.length <= usedLabels)
      {
        m_closeLabels.PushBack();

        vcLineInstance *pLineInstance = nullptr;
        vcLineRenderer_CreateLine(&pLineInstance);
        m_closeLines.PushBack(pLineInstance);
      }

      udDouble3 normal = vcGIS_GetWorldLocalUp(pProgramState->geozone, m_places[i].localSpace);

      m_closeLabels[usedLabels].worldPosition = m_places[i].localSpace + normal * 200.0;
      m_closeLabels[usedLabels].pText = m_places[i].pName;
      m_closeLabels[usedLabels].backColourRGBA = 0xFF000000;
      m_closeLabels[usedLabels].textColourRGBA = 0xFFFFFFFF;
      m_closeLabels[usedLabels].pSceneItem = this;

      udDouble3 points[] = { m_places[i].localSpace, m_closeLabels[usedLabels].worldPosition };
      vcLineRenderer_UpdatePoints(m_closeLines[usedLabels], points, 2, udFloat4::one(), 5, false);

      pRenderData->labels.PushBack(&m_closeLabels[usedLabels]);
      pRenderData->lines.PushBack(m_closeLines[usedLabels]);

      ++usedLabels;
    }
    else if (distSq < m_pinDistance * m_pinDistance)
    {
      pRenderData->pins.PushBack({ m_places[i].localSpace, m_pPinIcon, m_places[i].count, this });
    }

    // Models
    if (pPolygonModel != nullptr)
    {
      udDouble4x4 worldMatrix = udDouble4x4::translation(m_places[i].localSpace);
      udDouble4x4 worldViewProjectionMatrix = pProgramState->pActiveViewport->camera.matrices.viewProjection * worldMatrix;
      vcInstanceGeometryData data = { udFloat4x4::create(worldViewProjectionMatrix), udFloat4x4::create(worldMatrix), udFloat4::create(1,1,1,1), udFloat4::create(0,0,0,0) };
      pRenderData->instancedModels.PushData(pPolygonModel, &data);
    }
  }
}

void vcPlaceLayer::ApplyDelta(vcState * /*pProgramState*/, const udDouble4x4 & /*delta*/)
{
  // Does nothing
}

void vcPlaceLayer::HandleSceneExplorerUI(vcState * /*pProgramState*/, size_t *pItemID)
{
  double min = 0;
  double maxLabel = 50000;
  double maxPin = 6000000;

  if (ImGui::SliderScalar(udTempStr("Label Distance##%zu", *pItemID), ImGuiDataType_Double, &m_labelDistance, &min, &maxLabel))
    udProjectNode_SetMetadataDouble(m_pNode, "labelDistance", m_labelDistance);

  if (ImGui::SliderScalar(udTempStr("Pin Distance##%zu", *pItemID), ImGuiDataType_Double, &m_pinDistance, &min, &maxPin))
    udProjectNode_SetMetadataDouble(m_pNode, "pinDistance", m_pinDistance);
}

void vcPlaceLayer::ChangeProjection(const udGeoZone &newZone)
{
  for (size_t i = 0; i < m_places.length; ++i)
    m_places[i].localSpace = udGeoZone_LatLongToCartesian(newZone, m_places[i].latLongAlt);
}

void vcPlaceLayer::Cleanup(vcState * /*pProgramState*/)
{
  m_places.Deinit();
  m_closeLabels.Deinit();

  for (size_t i = 0; i < m_closeLines.length; ++i)
    vcLineRenderer_DestroyLine(&m_closeLines[i]);
  m_closeLines.Deinit();
}
