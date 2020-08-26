#ifndef vcBatchedRenderQueue_h__
#define vcBatchedRenderQueue_h__

#include "udChunkedArray.h"
#include "vcPolygonModel.h"

template <typename T>
struct vcBatchedRenderQueue
{
  struct Batch
  {
    vcPolygonModel *pModel; // TODO: Make more generic

    uint32_t count;
    uint32_t capacity;
    T *pData;
  };

  udResult Init(size_t batchCount);
  udResult Deinit();

  udResult PushData(vcPolygonModel *pModel, const T *pData);

  size_t GetBatchCount();
  const Batch* GetBatch(size_t index);

  udChunkedArray<Batch> batches;
  uint32_t maxBatchSize;
};

template <typename T>
inline udResult vcBatchedRenderQueue<T>::Init(size_t initialBatchCount)
{
  udResult result = udR_Success;

  UD_ERROR_CHECK(batches.Init(initialBatchCount));

  // TODO: this is graphics API specific? hardware specific?
  // on directx 10/11 compatible hardwave, the maximum constant buffer size is 65536 bytes.
  maxBatchSize = 65536 / sizeof(T);
epilogue:
  return result;
}

template <typename T>
inline udResult vcBatchedRenderQueue<T>::Deinit()
{
  for (size_t i = 0; i < batches.length; ++i)
    udFree(batches[i].pData);

  batches.Deinit();

  return udR_Success;
}

template <typename T>
inline udResult vcBatchedRenderQueue<T>::PushData(vcPolygonModel *pModel, const T *pData)
{
  // find an existing bin for pModel
  size_t i = 0;
  for (; i < batches.length; ++i)
  {
    vcBatchedRenderQueue::Batch *pBatch = &batches[i];
    if (pBatch->pModel == pModel && pBatch->count < pBatch->capacity)
    {
      memcpy(pBatch->pData + pBatch->count, pData, sizeof(T));
      ++pBatch->count;
      break;
    }
  }

  if (i == batches.length)
  {
    // Make a new queue
    vcBatchedRenderQueue::Batch *pBatch = batches.PushBack();
    pBatch->pModel = pModel;
    pBatch->capacity = maxBatchSize;
    pBatch->pData = udAllocType(T, pBatch->capacity, udAF_Zero);

    memcpy(pBatch->pData, pData, sizeof(T));
    ++pBatch->count;
  }

  // TODO: Error cases
  return udR_Success;
}

template <typename T>
inline size_t vcBatchedRenderQueue<T>::GetBatchCount()
{
  return batches.length;
}

template <typename T>
inline const typename vcBatchedRenderQueue<T>::Batch* vcBatchedRenderQueue<T>::GetBatch(size_t index)
{
  vcBatchedRenderQueue<T>::Batch* pBatch = &batches[index];
  return pBatch;
}

#endif//vcBatchedRenderQueue
