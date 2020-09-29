#ifndef vcCSV_h__
#define vcCSV_h__

#include "udResult.h"

struct udWorkerPool;

struct vcCSV
{
  udResult readResult;

  float *pEntries;
  int entryCount;
  int entryCapacity;
};

// If 'pWorkerPool' is nullptr, function will be synchronous
udResult vcCSV_Load(vcCSV **ppCSV, const char *pFilename, udWorkerPool *pWorkerPool);

void vcCSV_Destroy(vcCSV **ppCSV);

#endif// vcCSV_h__
