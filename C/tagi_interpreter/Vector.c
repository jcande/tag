#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <err.h>

#include "Vector.h"
#include "Util.h"

STATUS
VectorInitialize(
    Vector *Vec,
    void *Buffer,
    size_t Count,
    uint16_t ElementSize,
    TeardownFn Teardown
    )
{
    STATUS status = ENV_OK;
    size_t size = 0;
    void *buf = NULL;

    if (Vec == NULL)
    {
        TagWarnx("Vec must not be NULL");
        BAIL(status = ENV_BADPTR);
    }
    if (ElementSize == 0)
    {
        TagWarnx("ElementSize must be nonzero");
        BAIL(status = ENV_BADARG);
    }
    memset(Vec, 0, sizeof(*Vec));

    if (Count == 0)
    {
        //
        // Choose a sane default starting size.
        //
        size = ElementSize * 4;
    }
    else if (__builtin_mul_overflow(Count, ElementSize, &size))
    {
        TagWarnx("Count*ElementSize overflows");
        BAIL(status = ENV_INT_OVERFLOW);
    }

    buf = malloc(size);
    if (buf == NULL)
    {
        TagWarn("malloc buf");
        BAIL(status = ENV_OOM);
    }

    if (Buffer != NULL && Count != 0)
    {
        memcpy(buf, Buffer, size);
    }

    Vec->Buffer = buf;
    Vec->BufferSize = size;
    Vec->ActiveSize = 0;
    Vec->ElementSize = ElementSize;
    Vec->Teardown = Teardown;

Bail:
    if (FAILED(status))
    {
        free(buf);
    }

    return status;
}

bool
VectorHasRoom(
    Vector *Vec
    )
{
    assert(Vec != NULL);

    return (Vec->ActiveSize < Vec->BufferSize);
}

void *
VectorBuffer(
    Vector *Vec
    )
{
    assert(Vec != NULL);

    return Vec->Buffer;
}

size_t
VectorSize(
    Vector *Vec
    )
{
    assert(Vec != NULL);

    return (Vec->ActiveSize / Vec->ElementSize);
}

STATUS
VectorExpand(
    Vector *Vec,
    size_t MinimumSize
    )
{
    STATUS status = ENV_OK;
    size_t newSize = 0;
    void *newBuf = NULL;

    if (__builtin_mul_overflow(Vec->BufferSize, 4, &newSize))
    {
        newSize = MinimumSize;
    }

    newBuf = malloc(newSize);
    if (newBuf == NULL)
    {
        TagWarn("malloc newBuf");
        BAIL(status = ENV_OOM);
    }

    memcpy(newBuf, Vec->Buffer, Vec->ActiveSize);
    free(Vec->Buffer);
    Vec->Buffer = newBuf;
    Vec->BufferSize = newSize;

Bail:
    return status;
}

STATUS
VectorAppend(
    Vector *Vec,
    void *Element
    )
{
    STATUS status = ENV_OK;

    if (!VectorHasRoom(Vec))
    {
        size_t minSize;
        if (__builtin_add_overflow(Vec->BufferSize, Vec->ElementSize, &minSize))
        {
            TagWarnx("Cannot safely add another element");
            BAIL(status = ENV_INT_OVERFLOW);
        }

        CHECK(status = VectorExpand(Vec, minSize));
    }

    memcpy(&Vec->Buffer[Vec->ActiveSize], Element, Vec->ElementSize);
    Vec->ActiveSize += Vec->ElementSize;

Bail:
    return status;
}

void
VectorTeardown(
    Vector *Vec
    )
{
    if (Vec->Teardown != NULL)
    {
        size_t i;
        for (i = 0; i < Vec->ActiveSize; i += Vec->ElementSize)
        {
            Vec->Teardown(&Vec->Buffer[i]);
        }
    }

    free(Vec->Buffer);

    Vec->BufferSize = 0;
}
