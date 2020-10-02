#include "vcTiffHandler.h"
#include "udPlatform.h"

struct vcMemoryStream
{
  const uint8_t *pData;
  uint64_t size;
  uint64_t pos;
};

enum vcSeekDir
{
  vcSD_Beg,
  vcSD_Cur,
  vcSD_End
};

//-----------------------------------------------------------------
// Tiff client methods
//-----------------------------------------------------------------

static tsize_t vcTiffHandler_MemoryStream_Read(thandle_t fd, tdata_t buf, tsize_t size)
{
  vcMemoryStream *pData = reinterpret_cast<vcMemoryStream *>(fd);

  uint64_t remain = pData->size - pData->pos;
  uint64_t actual = remain < (uint64_t)size ? remain : (uint64_t)size;
  memcpy(buf, pData->pData + pData->pos, actual);
  pData->pos += actual;
  return actual;
}

// TODO Implement...
static tsize_t vcTiffHandler_MemoryStream_Write(thandle_t /*fd*/, tdata_t /*buf*/, tsize_t /*size*/)
{
  return 0;
}

// TODO Support seeking past the end of the memory block
static toff_t vcTiffHandler_MemoryStream_Seek(thandle_t fd, toff_t offset, int origin)
{
  vcMemoryStream *pData = reinterpret_cast<vcMemoryStream *>(fd);

  uint64_t newPos = 0;
  switch (origin)
  {
    case vcSD_Beg:
    {
      newPos = offset;
      break;
    }
    case vcSD_Cur:
    {
      newPos = pData->pos + offset;
      break;
    }
    case vcSD_End:
    {
      newPos = pData->size + offset;
      break;
    }
    default:
    {
      // Error...
      return 0;
    }
  }

  toff_t result = 0;
  if (newPos < pData->size)
  {
    pData->pos = newPos;
    result = offset;
  }
  return result;
}

static toff_t vcTiffHandler_MemoryStream_Size(thandle_t fd)
{
  vcMemoryStream *pData = reinterpret_cast<vcMemoryStream *>(fd);
  return toff_t(pData->size);
}

static int vcTiffHandler_MemoryStream_Close(thandle_t /*fd*/)
{
  // Do nothing...
  return 0;
}

static int vcTiffHandler_MemoryStream_Map(thandle_t /*fd*/, tdata_t * /*phase*/, toff_t * /*psize*/)
{
  return 0;
}

static void vcTiffHandler_MemoryStream_Unmap(thandle_t /*fd*/, tdata_t /*base*/, toff_t /*size*/)
{

}


udResult vcTiffHandler_CreateMemoryInStream(const uint8_t *pData, size_t size, vcMemoryStream **ppStream)
{
  udResult result;

  UD_ERROR_IF(pData == nullptr, udR_InvalidParameter_);
  UD_ERROR_IF(ppStream == nullptr, udR_InvalidParameter_);
  *ppStream = udAllocType(vcMemoryStream, 1, udAF_Zero);
  UD_ERROR_NULL(*ppStream, udR_MemoryAllocationFailure);

  (*ppStream)->pData = pData;
  (*ppStream)->pos = 0;
  (*ppStream)->size = size;

  result = udR_Success;
epilogue:
  return result;
}

void vcTiffHandler_DestroyMemoryStream(vcMemoryStream **ppStream)
{
  if (ppStream != nullptr)
    udFree(*ppStream);
}

udResult vcTiffHandler_CreateTiffInStream(vcMemoryStream *pStream, TIFF **ppTiff)
{
  udResult result;
  TIFF *pTiff = nullptr;

  UD_ERROR_NULL(ppTiff, udR_InvalidParameter_);
  UD_ERROR_NULL(pStream, udR_InvalidParameter_);

  pTiff = TIFFClientOpen("MEMTIFF", "r", (thandle_t)pStream,
                          vcTiffHandler_MemoryStream_Read,
                          vcTiffHandler_MemoryStream_Write,
                          vcTiffHandler_MemoryStream_Seek,
                          vcTiffHandler_MemoryStream_Close,
                          vcTiffHandler_MemoryStream_Size,
                          vcTiffHandler_MemoryStream_Map,
                          vcTiffHandler_MemoryStream_Unmap );

  UD_ERROR_NULL(pTiff, udR_OpenFailure);

  *ppTiff = pTiff;
  result = udR_Success;
epilogue:
  return result;
}
