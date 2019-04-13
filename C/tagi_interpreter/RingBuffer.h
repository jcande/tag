#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "Util.h"


#define RINGSS                  3
#define RINGSS_STATUS(Code)     (MAKE_STATUS(Code, RINGSS))

#define RINGSS_OK               0
#define RINGSS_FULL             (RINGSS_STATUS(1))

typedef enum {
    RingBuffer_Static = 0,
    RingBuffer_Expandable = 1,
    RingBuffer_Max
} RingBuffer_t;

typedef struct _RingBuffer
{
    uint8_t *Buffer;
    uint64_t BufferSize;

    // These range from 0..(BufferSize-1)
    uint64_t Head, Tail;
    uint64_t ActiveSize;

    RingBuffer_t Type;
} RingBuffer;

int
RingBufferInitialize(
    RingBuffer *Ring,
    uint64_t Size,
    RingBuffer_t Type
    );

int
RingBufferIsInitialized(
    RingBuffer *Ring
    );

void
RingBufferTeardown(
    RingBuffer *Ring
    );

int
RingBufferExpand(
    RingBuffer *Ring,
    uint64_t MinimumSize
    );

int
RingBufferPush(
    RingBuffer *Ring,
    void *Data,
    uint64_t DataSize
    );

int
RingBufferPop(
    RingBuffer *Ring,
    void *Data,
    uint64_t DataSize
    );

int
RingBufferPeek(
    RingBuffer *Ring,
    void *Data,
    uint64_t DataSize
    );

bool
RingBufferIsEmpty(
    RingBuffer *Ring
    );

void
RingBufferDump(
    RingBuffer *Ring
    );
