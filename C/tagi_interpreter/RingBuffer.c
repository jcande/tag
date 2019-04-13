#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <assert.h>

#include <err.h>

#include <stdio.h>

#include "RingBuffer.h"
#include "Util.h"

int
RingBufferInitialize(
    RingBuffer *Ring,
    uint64_t Size,
    RingBuffer_t Type
    )
{
    int status = 0;

    memset(Ring, 0, sizeof(*Ring));

    Ring->Head = Ring->Tail = 0;
    Ring->ActiveSize = 0;

    if (Type < 0 || Type >= RingBuffer_Max)
    {
        status = ENV_BADENUM;
        TagWarnx("Invalid enum value");
        goto Bail;
    }
    Ring->Type = Type;

    if (Size == 0)
    {
        //
        // Having a size of zero will result in undefined behavior when we
        // push/pop from the ring buffer as we are eventually performing an
        // operationg that uses % with a zero modulus. We will go ahead and
        // allocate a small amount just so the rest of the code's assumptions
        // remain valid.
        //

        Size = 0x10;
    }

    Ring->BufferSize = Size;
    Ring->Buffer = malloc(Ring->BufferSize);
    if (Ring->Buffer == NULL)
    {
        status = ENV_OOM;
        TagWarnx("malloc Ring->Buffer (%lu)", Ring->BufferSize);
        goto Bail;
    }

Bail:
    return status;
}

int
RingBufferIsInitialized(
    RingBuffer *Ring
    )
{
    return (Ring->Buffer != NULL);
}

void
RingBufferTeardown(
    RingBuffer *Ring
    )
{
    free(Ring->Buffer);
    Ring->Buffer = NULL;
    Ring->BufferSize = 0;
}

int
RingBufferExpand(
    RingBuffer *Ring,
    uint64_t MinimumSize
    )
{
    uint64_t size;
    RingBuffer newRing;
    int status = 0;

    if (__builtin_mul_overflow(Ring->BufferSize, 3, &size))
    {
        // XXX We should probably do something more/better than this.
        size = MinimumSize;
    }

    //
    // Allocate a bigger new ring.
    //
    status = RingBufferInitialize(&newRing, size, Ring->Type);
    if (FAILED(status))
    {
        goto Bail;
    }

    //
    // Copy over the data.
    //
    status = RingBufferPeek(Ring, newRing.Buffer, Ring->ActiveSize);
    if (FAILED(status))
    {
        goto Bail;
    }

    //
    // Update the state. We are guaranteed to have this data be contiguous
    // based on how RingBufferPeek works. Because of this, Head is just past
    // what has been written and Tail is 0 as nothing has been read yet.
    //
    newRing.Head = Ring->ActiveSize;
    newRing.Tail = 0;
    newRing.ActiveSize = Ring->ActiveSize;

    //
    // Success. Update the real ring.
    //
    RingBufferTeardown(Ring);
    *Ring = newRing;
    memset(&newRing, 0, sizeof(newRing));

Bail:
    if (RingBufferIsInitialized(&newRing))
    {
        RingBufferTeardown(&newRing);
    }

    return status;
}

int
RingBufferPush(
    RingBuffer *Ring,
    void *Data,
    uint64_t DataSize
    )
{
    int status = 0;
    uint64_t newSize;
    uint64_t slackspace;
    uint8_t *data = (uint8_t *) Data;

    assert(Ring->Buffer != NULL);
    assert(Ring->BufferSize > 0);

    if (__builtin_add_overflow(Ring->ActiveSize, DataSize, &newSize))
    {
        status = ENV_INT_OVERFLOW;
        TagWarnx("DataSize (%lu) overflows Ring->ActiveSize (%lu)", DataSize, Ring->ActiveSize);
        goto Bail;
    }
    if (newSize > Ring->BufferSize)
    {
        if (Ring->Type != RingBuffer_Expandable)
        {
            status = RINGSS_FULL;
            TagWarnx("Ring is full");
            goto Bail;
        }

        status = RingBufferExpand(Ring, newSize);
        if (FAILED(status))
        {
            goto Bail;
        }
    }

    slackspace = Ring->BufferSize - Ring->Head;
    if (DataSize > slackspace)
    {
        //
        // Head is too close to the end of the buffer so we'll have to copy the
        // data in two parts.
        //
        
        memcpy(&Ring->Buffer[Ring->Head], &data[0], slackspace);
        memcpy(&Ring->Buffer[0], &data[slackspace], DataSize - slackspace);
    }
    else
    {
        memcpy(&Ring->Buffer[Ring->Head], data, DataSize);
    }

    Ring->Head = (Ring->Head + DataSize) % Ring->BufferSize;
    Ring->ActiveSize = newSize;

Bail:
    return status;
}

int
RingBufferPop(
    RingBuffer *Ring,
    void *Data,
    uint64_t DataSize
    )
{
    int status = 0;
    uint8_t *data = (uint8_t *) Data;

    assert(Ring != NULL);
    assert(Ring->BufferSize > 0);

    status = RingBufferPeek(Ring, data, DataSize);
    if (FAILED(status))
    {
        goto Bail;
    }

    //
    // Actually pop the data from the ring.
    //

    Ring->Tail = (Ring->Tail + DataSize) % Ring->BufferSize;
    Ring->ActiveSize -= DataSize;

Bail:
    return status;
}

int
RingBufferPeek(
    RingBuffer *Ring,
    void *Data,
    uint64_t DataSize
    )
{
    int status = 0;
    uint64_t slackspace;
    uint8_t *data = (uint8_t *) Data;

    assert(Ring->Buffer != NULL);
    assert(Ring->BufferSize >= Ring->ActiveSize);
    assert(Data != NULL);

    if (DataSize > Ring->ActiveSize)
    {
        status = ENV_LENTOOBIG;
        TagWarnx("DataSize (%lu) > Ring->ActiveSize (%lu)", DataSize, Ring->ActiveSize);
        goto Bail;
    }

    slackspace = Ring->BufferSize - Ring->Tail;
    if (DataSize > slackspace)
    {
        //
        // Tail is too close to the start of the buffer so we'll have to copy
        // the data in two parts.
        //

        memcpy(&data[0],          &Ring->Buffer[Ring->Tail], slackspace);
        memcpy(&data[slackspace], &Ring->Buffer[0],          DataSize - slackspace);
    }
    else
    {
        memcpy(data, &Ring->Buffer[Ring->Tail], DataSize);
    }

Bail:
    return status;
}

bool
RingBufferIsEmpty(
    RingBuffer *Ring
    )
{
    assert(Ring != NULL);
    assert(Ring->Buffer != NULL);

    return (Ring->ActiveSize == 0);
}

void
RingBufferDump(
    RingBuffer *Ring
    )
{
    assert(Ring->Buffer != NULL);

    TagPrint("Head: %ju, Tail: %ju\n", Ring->Head, Ring->Tail);
    TagPrint("ActiveSize: %ju\n", Ring->ActiveSize);
    hexdump("Ring", Ring->Buffer, Ring->BufferSize);
}

#if 0
int
main(
    void
    )
{
    RingBuffer ring;
    int status;

    status = RingBufferInitialize(&ring, 24, RingBuffer_Static);
    if (status < 0)
    {
        printf("Initialize failed: %d\n", status);
        goto Bail;
    }

    char data[] = { 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40 };
    char data2[sizeof(data)];
    int i, j;
    for (i = 0; i < 5; ++i)
    {
        for (j = 0; j < sizeof(data); ++j) data[j]++;

        status = RingBufferPush(&ring, data, sizeof(data));
        if (status < 0)
        {
            printf("Push failed: %d\n", status);
            //goto Bail;
        }

        if ((i % 2) == 0)
        {
            status = RingBufferPop(&ring, data2, sizeof(data2));
            if (status < 0)
            {
                printf("Pop failed: %d\n", status);
                //goto Bail;
            }
            else
            {
                hexdump("Popped", data2, sizeof(data2));
            }
        }

        printf("\n--\n");
    }

    RingBufferDump(&ring);

Bail:
    return (status != 0);
}
#endif
