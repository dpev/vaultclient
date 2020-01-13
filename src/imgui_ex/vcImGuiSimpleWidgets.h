#ifndef vcImGuiSimpleWidgets_h__
#define vcImGuiSimpleWidgets_h__

#include "vcMath.h"
#include "vcSceneItem.h"
#include "imgui.h"

bool vcIGSW_InputText(const char *pLabel, char *pBuffer, size_t bufferSize, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
bool vcIGSW_InputTextWithResize(const char *pLabel, char **ppBuffer, size_t *pBufferSize, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
bool vcIGSW_ColorPickerU32(const char *pLabel, uint32_t *pColor, ImGuiColorEditFlags flags);
bool vcIGSW_StickyIntSlider(const char* label, int* v, int v_min, int v_max, int sticky);

udFloat4 vcIGSW_BGRAToImGui(uint32_t lineColour);
uint32_t vcIGSW_BGRAToRGBAUInt32(uint32_t lineColour);

bool vcIGSW_IsItemHovered(ImGuiHoveredFlags flags = 0, float timer = 0.5f);

void vcIGSW_ShowLoadStatusIndicator(vcSceneLoadStatus loadStatus, bool sameLine = true);

#endif // vcImGuiSimpleWidgets_h__
