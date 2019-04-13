#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <err.h>

#include "Blob.h"
#include "Util.h"

STATUS
BlobCopy(
    Blob *Destination,
    Blob *Source
    )
{
    STATUS status = ENV_OK;

    if (Destination->MaxSize < Source->Size)
    {
        uint8_t *newData = malloc(Source->Size);
        if (newData == NULL)
        {
            status = ENV_OOM;
            TagWarnx("malloc newData");
            goto Bail;
        }

        free(Destination->Data);
        Destination->Data = newData;
        Destination->MaxSize = Source->Size;
    }

    memcpy(Destination->Data, Source->Data, Source->Size);
    Destination->Size = Source->Size;

Bail:
    return status;
}

STATUS
BlobDupe(
    Blob *Source,
    Blob **Destination
    )
{
    STATUS status = ENV_OK;
    Blob *obj;

    obj = NULL;

    if (Destination == NULL)
    {
        status = ENV_BADPTR;
        TagWarnx("NULL Destination");
        goto Bail;
    }

    obj = malloc(sizeof(*obj));
    if (obj == NULL)
    {
        status = ENV_OOM;
        TagWarnx("malloc obj");
        goto Bail;
    }

    obj->Size = obj->MaxSize = 0;
    obj->Data = NULL;

    status = BlobCopy(obj, Source);
    if (FAILED(status))
    {
        goto Bail;
    }

    *Destination = obj;
    obj = NULL;

Bail:
    if (obj != NULL)
    {
        free(obj);
    }

    return status;
}

int
BlobInitialize(
    Blob *Obj,
    uint8_t *Source,
    uint64_t Size
    )
{
    Blob tmp = {
        .Data = Source,
        .Size = Size,
        .MaxSize = Size
    };

    assert((Source != NULL) || (Size == 0));

    Obj->Data = NULL;
    Obj->Size = Obj->MaxSize = 0;

    return BlobCopy(Obj, &tmp);
}

STATUS
BlobResize(
    Blob *Obj,
    uint64_t Size
    )
{
    STATUS status = ENV_OK;

    if (Obj->MaxSize < Size)
    {
        uint8_t *data = malloc(Size);
        if (data == NULL)
        {
            status = ENV_OOM;
            TagWarnx("malloc data");
            goto Bail;
        }

        memcpy(data, Obj->Data, Obj->Size);
        free(Obj->Data);

        Obj->Data = data;
        Obj->MaxSize = Size;
    }

    memset(&Obj->Data[Obj->Size], 0, Size - Obj->Size);
    Obj->Size = Size;

Bail:
    return status;
}

void
BlobTeardown(
    Blob *Obj
    )
{
    if (Obj == NULL)
    {
        return;
    }

    free(Obj->Data);
    Obj->Data = NULL;
    Obj->Size = Obj->MaxSize = 0;
}
