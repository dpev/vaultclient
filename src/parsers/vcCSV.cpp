#include "vcCSV.h"
#include "udPlatformUtil.h"
#include "udStringUtil.h"
#include "udWorkerPool.h"
#include "udFile.h"

struct AsyncCSVLoadInfo
{
  vcCSV *pCSV;
  const char* pFilename;
};

udResult vcCSV_Load(vcCSV *pCSV, const char *pFilename)
{
  udResult result;
  char *pMemory = nullptr;
  int64_t fileLen = 0;
  uint64_t start = udPerfCounterStart();
  const int initialCount = 1000;
  size_t delimiterIndex = -1;
  float loadTimeMS = 0.0f;

  UD_ERROR_NULL(pCSV, udR_InvalidParameter_);
  UD_ERROR_NULL(pFilename, udR_InvalidParameter_);

  UD_ERROR_CHECK(udFile_Load(pFilename, &pMemory, &fileLen));

  // TODO: '!' comment
  pCSV->pEntries = udAllocType(float, initialCount, udAF_Zero);
  pCSV->entryCount = 0;
  pCSV->entryCapacity = initialCount;

  while (pMemory[0] != '\0' && fileLen > 0 && udStrchr(pMemory, ",\r\n", &delimiterIndex) != nullptr)
  {
    if (pCSV->entryCount >= pCSV->entryCapacity)
    {
      pCSV->entryCapacity *= 2;
      pCSV->pEntries = udReallocType(pCSV->pEntries, float, pCSV->entryCapacity);
    }
    pCSV->pEntries[pCSV->entryCount] = udStrAtof(pMemory);
    pCSV->entryCount++;

    int len = delimiterIndex + 1;
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
  loadTimeMS = udPerfCounterMilliseconds(start);

  result = udR_Success;
epilogue:

  if (pCSV)
    pCSV->readResult = result;

  return result;
}

void vcCSV_AsyncLoadWorkerThreadWork(void* pLoadInfo)
{
  udResult result;
  AsyncCSVLoadInfo* pCSVLoadInfo = (AsyncCSVLoadInfo*)pLoadInfo;

  result = vcCSV_Load(pCSVLoadInfo->pCSV, pCSVLoadInfo->pFilename);
epilogue:

  udFree(pCSVLoadInfo->pFilename);
}

udResult vcCSV_Load(vcCSV **ppCSV, const char *pFilename, udWorkerPool *pWorkerPool)
{
  udResult result;
  AsyncCSVLoadInfo* pLoadInfo = nullptr;
  vcCSV *pCSV = nullptr;

  UD_ERROR_NULL(ppCSV, udR_InvalidParameter_);
  UD_ERROR_NULL(pFilename, udR_InvalidParameter_);

  pCSV = udAllocType(vcCSV, 1, udAF_Zero);
  UD_ERROR_NULL(pCSV, udR_MemoryAllocationFailure);

  pCSV->readResult = udR_Pending;

  if (pWorkerPool != nullptr)
  {
    pLoadInfo = udAllocType(AsyncCSVLoadInfo, 1, udAF_Zero);
    UD_ERROR_NULL(pLoadInfo, udR_MemoryAllocationFailure);

    pLoadInfo->pCSV = pCSV;
    pLoadInfo->pFilename = udStrdup(pFilename);

    UD_ERROR_CHECK(udWorkerPool_AddTask(pWorkerPool, vcCSV_AsyncLoadWorkerThreadWork, pLoadInfo, true));

  }
  else
  {
    // synchronous
    UD_ERROR_CHECK(vcCSV_Load(pCSV, pFilename));
  }

  result = udR_Success;
  *ppCSV = pCSV;
  pCSV = nullptr;
epilogue:

  if (pCSV != nullptr)
    pCSV->readResult = result;
  
  return result;
}

void vcCSV_Destroy(vcCSV **ppCSV)
{

}
