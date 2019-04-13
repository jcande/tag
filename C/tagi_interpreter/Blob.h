#pragma once

#include <stdint.h>

#include "Util.h"

typedef struct _Blob
{
    uint8_t *Data;
    uint64_t Size; // the active, or current, size
    uint64_t MaxSize;
} Blob;


STATUS
BlobCopy(
    Blob *Destination,
    Blob *Source
    );

STATUS
BlobDupe(
    Blob *Source,
    Blob **Destination
    );

STATUS
BlobInitialize(
    Blob *Obj,
    uint8_t *Source,
    uint64_t Size
    );

STATUS
BlobResize(
    Blob *Obj,
    uint64_t Size
    );

void
BlobTeardown(
    Blob *Obj
    );
