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
  //char *pMemory = nullptr;
  int64_t fileLen = 0;
  uint64_t start = udPerfCounterStart();
  const int initialCount = 1000;
  float loadTimeMS = 0.0f;
  udFile *pFile = nullptr;

  const int maxChunkSizeBytes = 1024 * 2;
  char chunkMemory[maxChunkSizeBytes] = {};
  int bytesRemaining = 0; // from previous chunk

    // here for debugging
  const char *pCur = chunkMemory;
  size_t delimiterIndex = -1;
  size_t actualRead = -1;

  UD_ERROR_NULL(pCSV, udR_InvalidParameter_);
  UD_ERROR_NULL(pFilename, udR_InvalidParameter_);

  UD_ERROR_CHECK(udFile_Open(&pFile, pFilename, udFOF_Read, &fileLen));

  pCSV->pEntries = udAllocType(float, initialCount, udAF_Zero);
  pCSV->entryCount = 0;
  pCSV->entryCapacity = initialCount;

  while (udFile_Read(pFile, chunkMemory + bytesRemaining, maxChunkSizeBytes - bytesRemaining, 0, udFSW_SeekCur, &actualRead) == udR_Success)
  {
    pCur = chunkMemory; // temp
    delimiterIndex = -1; // temp

    int bytesRead = 0;

    // TODO: '!' comment?
    while (pCur[bytesRead] == '\r' || pCur[bytesRead] == '\n' || pCur[bytesRead] == '\0')
      ++bytesRead;
    pCur += bytesRead;
    fileLen -= bytesRead;

    if (fileLen == 14463)
    {
      printf("B\n");
    }

    while (udStrchr(pCur, ",\r\n", &delimiterIndex) != nullptr)
    {
      if (pCSV->entryCount >= pCSV->entryCapacity)
      {
        pCSV->entryCapacity *= 2;
        pCSV->pEntries = udReallocType(pCSV->pEntries, float, pCSV->entryCapacity);
      }
      pCSV->pEntries[pCSV->entryCount] = udStrAtof(pCur);
      pCSV->entryCount++;

      int skipBytes = delimiterIndex + 1;
      while (bytesRead + skipBytes <= actualRead && (pCur[skipBytes] == '\r' || pCur[skipBytes] == '\n' || pCur[skipBytes] == '\0'))
        ++skipBytes;

      pCur += skipBytes;
      fileLen -= skipBytes;
      bytesRead += skipBytes;

      if (bytesRead >= actualRead)
        break;

      if (fileLen == 14463)
      {
        printf("A\n");
      }
    }

    if (bytesRead == actualRead)
    {
      printf("yp\n");
    }

    bytesRemaining = (actualRead - bytesRead);
    if (fileLen == bytesRemaining)
    {
      // read last entry
      pCSV->pEntries[pCSV->entryCount] = udStrAtof(pCur);
      pCSV->entryCount++;
      break;
    }

    // put remainder in front
    memmove(chunkMemory, chunkMemory + bytesRead, bytesRemaining);
    memset(chunkMemory + bytesRemaining, 0, actualRead - bytesRemaining);
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
