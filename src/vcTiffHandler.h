#ifndef vcTiffHandler_h__
#define vcTiffHandler_h__

#include <stdint.h>
#include "tiffio.h"
#include "udResult.h"

struct vcMemoryStream;

// Does not duplicate pData. Does not currently support writing
udResult vcTiffHandler_CreateMemoryInStream(const uint8_t *pData, size_t size, vcMemoryStream **ppStream);
void vcTiffHandler_DestroyMemoryStream(vcMemoryStream **ppStream);

udResult vcTiffHandler_CreateTiffInStream(vcMemoryStream *pStream, TIFF **ppTiff);

#endif
