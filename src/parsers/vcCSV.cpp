#include "vcCSV.h"
#include "udPlatformUtil.h"
#include "udStringUtil.h"
#include "udWorkerPool.h"
#include "udFile.h"

struct AsyncCSVLoadInfo
{
  udResult loadResult;
  vcCSV **ppCSV;

  const char* pFilename;
};

void vcCSV_Load(vcCSV **ppCSV, const char *pFilename)
{
  vcCSV *pCSV = udAllocType(vcCSV, 1, udAF_Zero);
  char* pMemory = nullptr;
  int64_t fileLen = 0;

  udResult error = udFile_Load(pFilename, &pMemory, &fileLen);

  // UD_ERROR_CHECK(error etc.)
  uint64_t start = udPerfCounterStart();

  // '!' comment
  const int initialCount = 1000;
  pCSV->pEntries = udAllocType(float, initialCount, udAF_Zero);
  pCSV->entryCount = 0;
  pCSV->entryCapacity = initialCount;

  size_t index = -1;
  while (pMemory[0] != '\0' && fileLen > 0 && udStrchr(pMemory, ",\r\n", &index) != nullptr)
  {
    if (pCSV->entryCount >= pCSV->entryCapacity)
    {
      pCSV->entryCapacity *= 2;
      pCSV->pEntries = udReallocType(pCSV->pEntries, float, pCSV->entryCapacity);
    }
    pCSV->pEntries[pCSV->entryCount] = udStrAtof(pMemory);
    pCSV->entryCount++;

    int len = index + 1;
    pMemory += len;
    fileLen -= len;
    if (fileLen <= 0)
      break;

    while (pMemory[0] == '\r' || pMemory[0] == '\n' || pMemory[0] == '\0')
    {
      --fileLen;
      pMemory++;
    }
  }

  float timeMS = udPerfCounterMilliseconds(start);

  pCSV->readResult = udR_Success;
  *ppCSV = pCSV;
}

void vcCSV_AsyncLoadWorkerThreadWork(void* pLoadInfo)
{
  udResult result;
  AsyncCSVLoadInfo* pCSVLoadInfo = (AsyncCSVLoadInfo*)pLoadInfo;

  vcCSV_Load(pCSVLoadInfo->ppCSV, pCSVLoadInfo->pFilename);

  //UD_ERROR_CHECK(udFile_Load(udTempStr("%s%s", pLoadInfo->pVertexFilename, pVertexFileExtension), &pLoadInfo->pVertexShaderText));
  //UD_ERROR_CHECK(udFile_Load(udTempStr("%s%s", pLoadInfo->pFragmentFilename, pFragmentFileExtension), &pLoadInfo->pFragmentShaderText));

  result = udR_Success;
epilogue:

  pCSVLoadInfo->loadResult = result;
  udFree(pCSVLoadInfo->pFilename);
}

udResult vcCSV_Load(vcCSV **ppCSV, const char *pFilename, udWorkerPool *pWorkerPool)
{
  udResult result;
  AsyncCSVLoadInfo* pLoadInfo = nullptr;

  UD_ERROR_NULL(ppCSV, udR_InvalidParameter_);
  UD_ERROR_NULL(pFilename, udR_InvalidParameter_);

  if (pWorkerPool != nullptr)
  {
    pLoadInfo = udAllocType(AsyncCSVLoadInfo, 1, udAF_Zero);
    UD_ERROR_NULL(pLoadInfo, udR_MemoryAllocationFailure);

    pLoadInfo->ppCSV = ppCSV;
    pLoadInfo->pFilename = udStrdup(pFilename);

    UD_ERROR_CHECK(udWorkerPool_AddTask(pWorkerPool, vcCSV_AsyncLoadWorkerThreadWork, pLoadInfo, true));

  }
  else
  {
    // synchronously load now
    vcCSV_Load(ppCSV, pFilename);
  }

  result = udR_Success;
  *ppCSV = nullptr;
epilogue:

  return result;
}

void vcCSV_Destroy(vcCSV **ppCSV)
{

}
