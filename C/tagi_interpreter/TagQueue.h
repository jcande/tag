#pragma once

#include <stdbool.h>

#include "Blob.h"
#include "RingBuffer.h"

#define TAGQ                    2
#define TAGQ_STATUS(Code)       (MAKE_STATUS(Code, TAGQ))

#define TAGQ_OK                 0
#define TAGQ_QUEUE_TOO_SMALL    1


typedef struct _TagQueue
{
    uint32_t DeletionNumber;
    uint32_t SymbolSize;

    RingBuffer Queue;

    struct {
        Blob Symbol;
        uint64_t SymbolCount;  // XXX think of a better name capturing "total amount of symbols in the cache"
        bool Dirty;
    } Cache;
} TagQueue;

STATUS
TagQueueInitialize(
        TagQueue *Q,
        uint32_t DeletionNumber,
        uint32_t SymbolSize
    );

void
TagQueueTeardown(
    TagQueue *Q
    );

STATUS
TagQueuePush(
    TagQueue *Q,
    Blob *Appendant,
    uint64_t Repetitions
    );

STATUS
TagQueuePop(
    TagQueue *Q,
    Blob *Symbol,
    uint64_t *Repetitions
    );
