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

  uint64_t start = udPerfCounterStart();

  // '!' comment
  const int count = 100;
  pCSV->pEntries = udAllocType(float, count, udAF_Zero);
  pCSV->entryCount = 0;
  pCSV->entryCapacity = count;
  while (pMemory[0] != '\0' && fileLen > 0)
  {
    char *pTokens[count];
    udStrTokenSplit(pMemory, ",\r\n", pTokens, count);

    for (int i = 0; i < count; ++i)
    {
      if (pCSV->entryCount >= pCSV->entryCapacity)
      {
        pCSV->entryCapacity *= 2;
        pCSV->pEntries = udReallocType(pCSV->pEntries, float, pCSV->entryCapacity);
      }
      pCSV->pEntries[pCSV->entryCount] = udStrAtof(pMemory);
      pCSV->entryCount++;

      // relies on udStrTokenSplit() null terminating delimeters
      int len = udStrlen(pMemory) + 1;
      pMemory += len;
      fileLen -= len;
      if (fileLen <= 0)
        break;

      //size_t index = -1;
      //udStrchr(pMemory, ",", &index);
      //pMemory += index;
      while (pMemory[0] == '\r' || pMemory[0] == '\n' || pMemory[0] == '\0')
      {
        --fileLen;
        pMemory++;
      }
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
